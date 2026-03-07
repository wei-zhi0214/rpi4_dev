#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "/home/william/rpi4_dev/driver/motor/motor_pwm.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include "/home/william/rpi4_dev/driver/imu/imu_uapi.h"
#include "/home/william/rpi4_dev/driver/encoder/enc_uapi.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif
			     
#define ENCODER_CPR 1320.0

int rpm_updated = 0;

typedef struct {
    int fd;
    struct enc_sample prev;
    int inited;
    double rpm;              // latest rpm
    double rpm_raw;  // last raw rpm (optional)
    uint64_t ok;
    uint64_t eagain;
    uint64_t err;
    uint64_t shortread;
    int32_t acc_dcount;
    int64_t acc_dts_ns;
    int decim;   // e.g. 4
    int k;
} enc_state_t;


static int enc_open(enc_state_t *e, const char *path)
{
    memset(e, 0, sizeof(*e));
    e->fd = open(path, O_RDONLY | O_NONBLOCK);
    if (e->fd < 0) { perror(path); return -1; }

    // prime first sample
    struct enc_sample s;
    ssize_t n = read(e->fd, &s, sizeof(s));
    if (n != (ssize_t)sizeof(s)) {
        perror("encoder read first");
        close(e->fd);
        return -1;
    }
    e->prev = s;
    e->inited = 1;
    e->rpm = 0.0;
    e->acc_dcount = 0;
    e->acc_dts_ns = 0;
    e->decim = 4;   // 4*5ms = 20ms
    e->k = 0;

    return 0;
}

static inline double ns_to_s(int64_t ns) { return (double)ns / 1e9; }

static int enc_update(enc_state_t *e)
{
    struct enc_sample cur;
    ssize_t n = read(e->fd, &cur, sizeof(cur));
    if (n == (ssize_t)sizeof(cur)) {
        int32_t dcount = cur.count - e->prev.count;
        int64_t dts    = cur.ts_ns - e->prev.ts_ns;
        e->prev = cur;
        e->ok++;

        if (dts <= 0) return 0;

        e->acc_dcount += dcount;
        e->acc_dts_ns += dts;
        e->k++;

        if (e->k >= e->decim) {
            double dt = (double)e->acc_dts_ns / 1e9;
            double rev_per_s = ((double)e->acc_dcount / ENCODER_CPR) / dt;
            double rpm_raw = rev_per_s * 60.0;
            e->rpm_raw = rpm_raw;

            // IIR low-pass on rpm (helps kill quantization)
            double alpha = 0.25;                // 0.15~0.35
            e->rpm = e->rpm + alpha * (rpm_raw - e->rpm);

            e->acc_dcount = 0;
            e->acc_dts_ns = 0;
            e->k = 0;
            return 1; // rpm updated
        }
        return 0;
    }

    if (n < 0 && errno == EAGAIN) { e->eagain++; return 0; }
    if (n < 0) { e->err++; return 0; }
    e->shortread++;
    return 0;
}

typedef struct {
    // gains (v-controller): v is 0..1 "effort"
    double Kp_v;      // v per rpm error
    double Ki_v;      // v per (rpm*s)
    double Ts;        // seconds

    // deadzone mapping
    int u0_ns;        // e.g. 700000
    int umax_ns;      // e.g. 1000000

    // integrator
    double i_v;       // integrator state in v

    // kick-start
    int kick_u_ns;        // e.g. 1000000
    int kick_ms;          // e.g. 100
    int kick_left_ms;     // countdown

    // state
    bool was_stopped;
} speed_pi_t;

// init with 200Hz
static inline void speed_pi_init(speed_pi_t *c)
{
    c->Kp_v = 0.004;      // start safe (tune later)
    c->Ki_v = 0.050;      // start safe (tune later)
    c->Ts   = 0.005;      // 200 Hz

    c->u0_ns   = 700000;
    c->umax_ns = 1000000;

    c->i_v = 0.0;

    c->kick_u_ns = 1000000;
    c->kick_ms   = 100;
    c->kick_left_ms = 0;

    c->was_stopped = true;
}

// clamp helper
static inline double clampd(double x, double lo, double hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// Update: rpm_ref can be signed; rpm_meas signed too.
// Return: duty_ns command (signed)
static inline int speed_pi_update(speed_pi_t *c, double rpm_ref, double rpm_meas)
{
    // handle sign by controlling magnitude with PI, then apply sign
    double sgn = (rpm_ref >= 0.0) ? 1.0 : -1.0;
    double ref = (rpm_ref >= 0.0) ? rpm_ref : -rpm_ref;
    double mea = (rpm_meas >= 0.0) ? rpm_meas : -rpm_meas;

    // stopped detection
    bool stopped = (ref < 1e-3);
    if (stopped) {
        c->i_v = 0.0;
        c->kick_left_ms = 0;
        c->was_stopped = true;
        return 0;
    }

    // error in rpm
    double e = ref - mea;

    // PI in "effort" domain v (0..1)
    double p = c->Kp_v * e;
    // I only when rpm updated (20ms)
    if (rpm_updated) {
        double Ti = 0.020;  // 20ms
        c->i_v += c->Ki_v * e * Ti;
        c->i_v = clampd(c->i_v, 0.0, 1.0);
    }

    double v = p + c->i_v;
    v = clampd(v, 0.0, 1.0);

    // kick-start: from standstill to move
    if (c->was_stopped && mea < 5.0 && v > 0.05) {
        c->kick_left_ms = c->kick_ms;
        c->was_stopped = false;
    }
    if (mea >= 5.0) c->was_stopped = false;

    if (c->kick_left_ms > 0) {
        c->kick_left_ms -= 5; 
        if (c->kick_left_ms < 0) c->kick_left_ms = 0;
        return (int)(sgn * c->kick_u_ns);
    }

    // deadzone mapping
    int u_ns = 0;
    if (v <= 0.0) {
        u_ns = 0;
    } else {
        double u = (double)c->u0_ns + v * (double)(c->umax_ns - c->u0_ns);
        if (u > c->umax_ns) u = c->umax_ns;
        u_ns = (int)u;
    }

    return (int)(sgn * u_ns);
}

static inline uint64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline void ts_from_ns(uint64_t ns, struct timespec *ts) {
    ts->tv_sec = (time_t)(ns / 1000000000ULL);
    ts->tv_nsec = (long)(ns % 1000000000ULL);
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

static uint64_t percentile_u64(uint64_t *arr, size_t n, double p) {
    if (n == 0) return 0;
    double idx = p * (double)(n - 1);
    size_t i = (size_t)(idx + 0.5);
    if (i >= n) i = n - 1;
    return arr[i];
}

typedef struct {
    int hz;
    int cpu;
    int fifo_prio;
} cfg_t;

typedef struct {
    cfg_t cfg;
    volatile int stop;
} ctx_t;

static void bind_cpu(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
        perror("pthread_setaffinity_np");
        exit(1);
    }
}

static void set_fifo(int prio) {
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = prio;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        perror("pthread_setschedparam(SCHED_FIFO)");
        fprintf(stderr, "Hint: run as root or set capabilities; also check ulimit -r.\n");
        exit(1);
    }
}

static void lock_memory(void) {
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        perror("mlockall");
        fprintf(stderr, "Hint: check ulimit -l and CAP_IPC_LOCK.\n");
        exit(1);
    }
}

static void busy_work_us(int us) {
    // 模擬控制計算負載：用忙等避免 sleep 影響
    uint64_t start = nsec_now();
    uint64_t dur = (uint64_t)us * 1000ULL;
    while (nsec_now() - start < dur) { /* spin */ }
}

static void *ctrl_thread(void *arg) {
    ctx_t *c = (ctx_t*)arg;

    bind_cpu(c->cfg.cpu);
    set_fifo(c->cfg.fifo_prio);
    lock_memory();
	   
    int motor_fd = open("/dev/motor0", O_RDWR);
    if (motor_fd < 0) {
	    perror("open /dev/motor0");
	    return NULL;
    }
    //int imu_fd = open("/dev/imu0", O_RDONLY | O_NONBLOCK);
    //if (imu_fd < 0) {
	//    perror("open /dev/imu0");
	  //  return NULL;
    //}

    enc_state_t enc_l, enc_r;

    if (enc_open(&enc_l, "/dev/encoder-left") < 0) return NULL;
    //if (enc_open(&enc_r, "/dev/encoder-right") < 0) return NULL;

    const uint64_t period_ns = 1000000000ULL / (uint64_t)c->cfg.hz;

    // metrics：用 1 秒窗口收集
    const size_t WIN = 2000; // 足夠裝 1000Hz 以內
    uint64_t jit_ns[WIN];
    uint64_t exe_ns[WIN];
    size_t jit_n = 0, exe_n = 0;
    uint64_t miss_deadline = 0;

    uint64_t next_ns = nsec_now() + period_ns;
    uint64_t last_start = 0;
    uint32_t en = 1;
    static int32_t duty = 0;
    static int dir = 1;
    struct motor_cmd mc = {
	    .left  = duty,
	    .right = duty,
	    .flags = MOTOR_FLAG_ENABLE,
	    .reserved = 0,
    };
    //struct imu_sample imu_last = {0};
    //uint64_t imu_ok =0;
    //uint64_t imu_eagain =0;
    //uint64_t  imu_err = 0;
    ioctl(motor_fd, MOTOR_IOC_ENABLE, &en);
    //uint64_t age_ns = 0;
    //uint64_t max_age = 0;

    static speed_pi_t spd;

    speed_pi_init(&spd);

    double rpm_ref = 120.0;   // 先固定目標
    double rpm_meas = 0.0;
    while (!c->stop) {
        struct timespec ts;
        ts_from_ns(next_ns, &ts);

	struct imu_sample s;
	int got = 0;

        int rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
        if (rc != 0 && rc != EINTR) {
            errno = rc;
            perror("clock_nanosleep");
        }

        uint64_t t_start = nsec_now();

        if (last_start != 0 && jit_n < WIN) {
            uint64_t actual_period = t_start - last_start;
            uint64_t jitter = (actual_period > period_ns) ? (actual_period - period_ns)
                                                          : (period_ns - actual_period);
            jit_ns[jit_n++] = jitter;
        }
        last_start = t_start;

        // ---- 控制計算（目前先用假負載）----
        busy_work_us(300); // 0.3ms 負載，你可調成 0.1~2ms
        // ----------------------------------


//	for (;;) {
//		ssize_t n = read(imu_fd, &s, sizeof(s));
//		if (n == (ssize_t)sizeof(s)) {
//			imu_last = s;
//			got = 1;
//			imu_ok++;
//			continue;              // 還可能有更新的，繼續讀
//		}
//		if (n < 0 && errno == EAGAIN) {
//			if (!got) imu_eagain++; // 這圈完全沒有新資料
//			break;
//		}
//		if (n < 0) {
//			imu_err++;
//			break;
//		}
//		break;
//	}
	// 更新 encoder（用本圈的 t_end 或 t_start 都行；建議用 t_end 比較接近）
	rpm_updated = enc_update(&enc_l);

	rpm_meas = enc_l.rpm;

	/* 做個 sawtooth，避免真的轉太快 */
	// rpm_ref: 例如 120 rpm
	// rpm_meas: encoder 算出來的 rpm (signed)
	int u_ns = speed_pi_update(&spd, rpm_ref, rpm_meas);

	mc.left  = u_ns;    // 單輪：只推 left
	mc.right = 0;
	if (ioctl(motor_fd, MOTOR_IOC_SET, &mc) < 0) perror("MOTOR_IOC_SET");

	uint64_t t_end = nsec_now();
        uint64_t exec = t_end - t_start;
	//age_ns = t_end - imu_last.ts_ns;
	//max_age = max_age > age_ns ? max_age : age_ns;
        if (exe_n < WIN) exe_ns[exe_n++] = exec;

        // deadline miss：如果 exec 超過 period 或者已經超過下一個時刻太多
        if (exec > period_ns) miss_deadline++;

        // 每秒輸出一次統計
        static uint64_t last_report = 0;
        if (last_report == 0) last_report = t_start;

        if (t_start - last_report >= 1000000000ULL) {
            // sort for percentile
            qsort(jit_ns, jit_n, sizeof(uint64_t), cmp_u64);
            qsort(exe_ns, exe_n, sizeof(uint64_t), cmp_u64);

            uint64_t jit_p50  = percentile_u64(jit_ns, jit_n, 0.50);
            uint64_t jit_p99  = percentile_u64(jit_ns, jit_n, 0.99);
            uint64_t jit_p999 = percentile_u64(jit_ns, jit_n, 0.999);
            uint64_t jit_max  = (jit_n ? jit_ns[jit_n - 1] : 0);

            uint64_t exe_p99  = percentile_u64(exe_ns, exe_n, 0.99);
            uint64_t exe_p999 = percentile_u64(exe_ns, exe_n, 0.999);
            uint64_t exe_max  = (exe_n ? exe_ns[exe_n - 1] : 0);

	    printf("[RT] %d Hz | jitter(ns) p50=%"PRIu64" p99=%"PRIu64" p999=%"PRIu64" max=%"PRIu64
			    " | exec(ns) p99=%"PRIu64" p999=%"PRIu64" max=%"PRIu64
			    " | miss=%"PRIu64
			    " | rpm_ref=%.1f rpm=%.2f u=%d"
			    " | enc_ok=%"PRIu64" eagain=%"PRIu64" err=%"PRIu64" short=%"PRIu64"\n",
			    c->cfg.hz,
			    jit_p50, jit_p99, jit_p999, jit_max,
			    exe_p99, exe_p999, exe_max,
			    miss_deadline,
			    rpm_ref, rpm_meas, u_ns,
			    enc_l.ok, enc_l.eagain, enc_l.err, enc_l.shortread);

	    printf(" | rpm_ref=%.1f rpm=%.2f u=%d | enc_ok=%"PRIu64" err=%"PRIu64" short=%"PRIu64"\n",
			    rpm_ref, rpm_meas, mc.left, enc_l.ok, enc_l.err, enc_l.shortread);

	    fflush(stdout);

	    // reset window
	    jit_n = exe_n = 0;
            last_report = t_start;
        }

        next_ns += period_ns;

        // 若系統太卡導致 next_ns 已經在過去，視為 miss 並重新對齊
        uint64_t now = nsec_now();
        if (now > next_ns + period_ns) {
            miss_deadline++;
            next_ns = now + period_ns;
        }
    }

    return NULL;
}

int main(int argc, char **argv) {
    ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cfg.hz = 200;
    ctx.cfg.cpu = 3;
    ctx.cfg.fifo_prio = 80;

    // 可用參數：balancertd [hz] [cpu] [prio]
    if (argc > 1) ctx.cfg.hz = atoi(argv[1]);
    if (argc > 2) ctx.cfg.cpu = atoi(argv[2]);
    if (argc > 3) ctx.cfg.fifo_prio = atoi(argv[3]);

    printf("balancertd: hz=%d cpu=%d prio=%d\n", ctx.cfg.hz, ctx.cfg.cpu, ctx.cfg.fifo_prio);

    pthread_t th;
    if (pthread_create(&th, NULL, ctrl_thread, &ctx) != 0) {
        perror("pthread_create");
        return 1;
    }

    // 簡單跑 30 秒
    sleep(30);
    ctx.stop = 1;
    pthread_join(th, NULL);
    return 0;
}

