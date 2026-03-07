#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "/home/william/rpi4_dev/driver/motor/motor_pwm.h"
#include "/home/william/rpi4_dev/driver/encoder/enc_uapi.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

// JGB37-520: 11 lines, quadrature x4 => 44 counts/motor_rev
// gear 1:30 => 1320 counts/output_rev
#define ENCODER_CPR 1320.0

// ===================== time utils =====================
static inline uint64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline void ts_from_ns(uint64_t ns, struct timespec *ts) {
    ts->tv_sec  = (time_t)(ns / 1000000000ULL);
    ts->tv_nsec = (long)(ns % 1000000000ULL);
}

// ===================== RT helpers =====================
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

// (optional) simulate compute load
static void busy_work_us(int us) {
    uint64_t start = nsec_now();
    uint64_t dur = (uint64_t)us * 1000ULL;
    while (nsec_now() - start < dur) { /* spin */ }
}

// ===================== config =====================
typedef struct {
    int hz;
    int cpu;
    int fifo_prio;

    double rpm0, rpm1;      // step: rpm0 -> rpm1
    int pre_ms, hold_ms;    // step timing
    const char *csv_path;

    int enc_decim;          // 4 => 20ms if hz=200
} cfg_t;

typedef struct {
    cfg_t cfg;
    volatile int stop;
} ctx_t;

// ===================== encoder =====================
typedef struct {
    int fd;
    struct enc_sample prev;

    double rpm;       // filtered / used rpm
    double rpm_raw;   // raw rpm

    uint64_t ok, eagain, err, shortread;

    int32_t acc_dcount;
    int64_t acc_dts_ns;

    int decim;  // update every decim samples
    int k;
} enc_state_t;

static int enc_prime_first(enc_state_t *e, int timeout_ms) {
    uint64_t t0 = nsec_now();
    while (1) {
        struct enc_sample s;
        ssize_t n = read(e->fd, &s, sizeof(s));
        if (n == (ssize_t)sizeof(s)) {
            e->prev = s;
            return 0;
        }
        if (n < 0 && errno == EAGAIN) {
            // wait a bit
            usleep(1000);
        } else {
            perror("encoder prime read");
            return -1;
        }

        uint64_t now = nsec_now();
        if ((now - t0) > (uint64_t)timeout_ms * 1000000ULL) {
            fprintf(stderr, "encoder prime timeout\n");
            return -1;
        }
    }
}

static int enc_open(enc_state_t *e, const char *path, int decim) {
    memset(e, 0, sizeof(*e));
    e->fd = open(path, O_RDONLY | O_NONBLOCK);
    if (e->fd < 0) { perror(path); return -1; }

    e->decim = (decim > 0) ? decim : 4;
    e->k = 0;
    e->rpm = 0.0;
    e->rpm_raw = 0.0;
    e->acc_dcount = 0;
    e->acc_dts_ns = 0;

    if (enc_prime_first(e, 1000) < 0) {
        close(e->fd);
        return -1;
    }
    return 0;
}

// return 1 if rpm updated (every decim samples), else 0
static int enc_update(enc_state_t *e) {
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

            // optional low-pass; 0 => pure raw
            double alpha = 1;  // 0.15~0.35
            e->rpm = e->rpm + alpha * (rpm_raw - e->rpm);
            //e->rpm = rpm_raw;

            e->acc_dcount = 0;
            e->acc_dts_ns = 0;
            e->k = 0;
            return 1;
        }
        return 0;
    }

    if (n < 0 && errno == EAGAIN) { e->eagain++; return 0; }
    if (n < 0) { e->err++; return 0; }
    e->shortread++;
    return 0;
}

// ===================== speed PI controller =====================
typedef struct {
    double Kp_v;      // v per rpm error
    double Ki_v;      // v per (rpm*s)
    double Ts;        // controller base period (5ms at 200Hz)

    int u0_ns;        // deadzone base
    int umax_ns;      // max

    double i_v;       // integrator state in v

    // kick-start (can be disabled by kick_ms=0)
    int kick_u_ns;
    int kick_ms;
    int kick_left_ms;

    bool was_stopped;
} speed_pi_t;

static inline double clampd(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline void speed_pi_init(speed_pi_t *c, double Ts) {
    c->Kp_v = 0.004;     // 先用你原本較穩的
    c->Ki_v = 0.050;
    c->Ts   = Ts;

    c->u0_ns   = 700000;
    c->umax_ns = 1000000;

    c->i_v = 0.0;

    c->kick_u_ns = 1000000;
    c->kick_ms   = 100;
    c->kick_left_ms = 0;

    c->was_stopped = true;
}

// rpm_updated=1 時才積 I；Ti = Ts*decim
static inline int speed_pi_update(speed_pi_t *c,
                                  double rpm_ref, double rpm_meas,
                                  int rpm_updated, int decim, int allow_i)
{
    double sgn = (rpm_ref >= 0.0) ? 1.0 : -1.0;
    double ref = (rpm_ref >= 0.0) ? rpm_ref : -rpm_ref;
    double mea = (rpm_meas >= 0.0) ? rpm_meas : -rpm_meas;

    bool stopped = (ref < 1e-3);
    if (stopped) {
        c->i_v = 0.0;
        c->kick_left_ms = 0;
        c->was_stopped = true;
        return 0;
    }

    double e = ref - mea;

    // P always
    double p = c->Kp_v * e;

    // I only on fresh rpm update
    if (allow_i && rpm_updated) {
	    double Ti = c->Ts * (double)((decim > 0) ? decim : 1);
	    c->i_v += c->Ki_v * e * Ti;
	    c->i_v = clampd(c->i_v, 0.0, 1.0);
    }

    double v = p + c->i_v;
    v = clampd(v, 0.0, 1.0);

    // kick-start (optional)
    if (c->was_stopped && mea < 5.0 && v > 0.05) {
        c->kick_left_ms = c->kick_ms;
        c->was_stopped = false;
    }
    if (mea >= 5.0) c->was_stopped = false;

    if (c->kick_left_ms > 0) {
        c->kick_left_ms -= (int)(c->Ts * 1000.0 + 0.5);
        if (c->kick_left_ms < 0) c->kick_left_ms = 0;
        int u = (int)(sgn * c->kick_u_ns);
        if (u >  c->umax_ns) u =  c->umax_ns;
        if (u < -c->umax_ns) u = -c->umax_ns;
        return u;
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

    u_ns = (int)(sgn * u_ns);
    if (u_ns >  c->umax_ns) u_ns =  c->umax_ns;
    if (u_ns < -c->umax_ns) u_ns = -c->umax_ns;
    return u_ns;
}

// ===================== control thread =====================
static void *ctrl_thread(void *arg) {
    ctx_t *c = (ctx_t*)arg;

    bind_cpu(c->cfg.cpu);
    set_fifo(c->cfg.fifo_prio);
    lock_memory();

    int motor_fd = open("/dev/motor0", O_RDWR);
    if (motor_fd < 0) { perror("open /dev/motor0"); return NULL; }

    enc_state_t enc_l;
    if (enc_open(&enc_l, "/dev/encoder-left", c->cfg.enc_decim) < 0) return NULL;

    uint32_t en = 1;
    if (ioctl(motor_fd, MOTOR_IOC_ENABLE, &en) < 0) perror("MOTOR_IOC_ENABLE");

    const uint64_t period_ns = 1000000000ULL / (uint64_t)c->cfg.hz;

    FILE *fp = fopen(c->cfg.csv_path, "w");
    if (!fp) { perror("fopen csv"); return NULL; }

    fprintf(fp, "t_ms,rpm_ref,rpm,rpm_raw,u,updated,enc_ok,enc_eagain,enc_err,enc_short\n");
    fflush(fp);

    speed_pi_t spd;
    speed_pi_init(&spd, 1.0 / (double)c->cfg.hz);

    struct motor_cmd mc = {
        .left = 0, .right = 0,
        .flags = MOTOR_FLAG_ENABLE,
        .reserved = 0,
    };

    // ======= slew-rate limiter (超重要) =======
    static int u_last = 0;
    const int DU_MAX = 8000;  // 每 5ms 允許的最大變化量 (ns). overshoot大就降，反應太慢就升
    // ========================================

    uint64_t t0 = nsec_now();
    uint64_t end_ns = t0 + (uint64_t)(c->cfg.pre_ms + c->cfg.hold_ms) * 1000000ULL;

    uint64_t next_ns = t0 + period_ns;

    int flush_k = 0;

    while (!c->stop) {
        struct timespec ts;
        ts_from_ns(next_ns, &ts);

        int rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
        if (rc != 0 && rc != EINTR) {
            errno = rc;
            perror("clock_nanosleep");
        }

        uint64_t now_ns = nsec_now();
        double t_ms = (double)(now_ns - t0) / 1e6;

        // (optional) simulate compute load
        // busy_work_us(300);

        // step profile
        double rpm_ref = (t_ms < (double)c->cfg.pre_ms) ? c->cfg.rpm0 : c->cfg.rpm1;

        // encoder update
        int updated = enc_update(&enc_l);
        double rpm_meas = enc_l.rpm;

	// control
	// 先算 PI 輸出，但先不更新 I（allow_i=0）
	int u_pi = speed_pi_update(&spd, rpm_ref, rpm_meas, updated, enc_l.decim, 0);

	// 套 slew-rate limit
	int u_cmd = u_pi;
	int du = u_cmd - u_last;
	int slew_limited = 0;
	if (du > DU_MAX) { u_cmd = u_last + DU_MAX; slew_limited = 1; }
	else if (du < -DU_MAX) { u_cmd = u_last - DU_MAX; slew_limited = 1; }
	u_last = u_cmd;

	// 如果沒有被 slew-limit 卡住，才允許更新 I（allow_i=1）
	if (!slew_limited) {
		(void)speed_pi_update(&spd, rpm_ref, rpm_meas, updated, enc_l.decim, 1);
	}

	mc.left  = u_cmd;
        mc.right = 0;
        if (ioctl(motor_fd, MOTOR_IOC_SET, &mc) < 0) perror("MOTOR_IOC_SET");

        // CSV log (每 5ms 一行)
        fprintf(fp, "%.3f,%.3f,%.3f,%.3f,%d,%d,%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64"\n",
                t_ms, rpm_ref, rpm_meas, enc_l.rpm_raw,
                u_cmd, updated,
                enc_l.ok, enc_l.eagain, enc_l.err, enc_l.shortread);

        if (++flush_k >= 50) { fflush(fp); flush_k = 0; }

        if (now_ns >= end_ns) {
            fflush(fp);
            break;
        }

        next_ns += period_ns;

        // 若系統太卡 next_ns 落後太多，重對齊（避免爆掉）
        uint64_t n2 = nsec_now();
        if (n2 > next_ns + period_ns) {
            next_ns = n2 + period_ns;
        }
    }

    // stop motor
    mc.left = 0; mc.right = 0;
    ioctl(motor_fd, MOTOR_IOC_SET, &mc);
    fclose(fp);
    return NULL;
}

// ===================== main =====================
int main(int argc, char **argv) {
    ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    // defaults
    ctx.cfg.hz = 200;
    ctx.cfg.cpu = 3;
    ctx.cfg.fifo_prio = 80;

    ctx.cfg.rpm0 = 0.0;
    ctx.cfg.rpm1 = 120.0;
    ctx.cfg.pre_ms = 1000;
    ctx.cfg.hold_ms = 4000;
    ctx.cfg.csv_path = "motor_step.csv";

    ctx.cfg.enc_decim = 4; // 4*5ms=20ms (你的策略)

    // usage:
    // balancertd [hz] [cpu] [prio] [rpm0] [rpm1] [pre_ms] [hold_ms] [csv_path] [decim]
    if (argc > 1) ctx.cfg.hz = atoi(argv[1]);
    if (argc > 2) ctx.cfg.cpu = atoi(argv[2]);
    if (argc > 3) ctx.cfg.fifo_prio = atoi(argv[3]);
    if (argc > 4) ctx.cfg.rpm0 = atof(argv[4]);
    if (argc > 5) ctx.cfg.rpm1 = atof(argv[5]);
    if (argc > 6) ctx.cfg.pre_ms = atoi(argv[6]);
    if (argc > 7) ctx.cfg.hold_ms = atoi(argv[7]);
    if (argc > 8) ctx.cfg.csv_path = argv[8];
    if (argc > 9) ctx.cfg.enc_decim = atoi(argv[9]);

    printf("balancertd: hz=%d cpu=%d prio=%d | step %.1f->%.1f pre=%dms hold=%dms | csv=%s | decim=%d\n",
           ctx.cfg.hz, ctx.cfg.cpu, ctx.cfg.fifo_prio,
           ctx.cfg.rpm0, ctx.cfg.rpm1, ctx.cfg.pre_ms, ctx.cfg.hold_ms,
           ctx.cfg.csv_path, ctx.cfg.enc_decim);

    pthread_t th;
    if (pthread_create(&th, NULL, ctrl_thread, &ctx) != 0) {
        perror("pthread_create");
        return 1;
    }

    // 讓它跑到 thread 自己結束（pre+hold），外加保險
    int total_ms = ctx.cfg.pre_ms + ctx.cfg.hold_ms + 500;
    usleep(total_ms * 1000);

    ctx.stop = 1;
    pthread_join(th, NULL);
    return 0;
}

