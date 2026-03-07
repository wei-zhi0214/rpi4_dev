// balancertd_arch.c
// Dual-wheel test version (NO latch, NO slew)
// - uL/uR 分開輸出（左右各自 bias/delta_max/kick）
// - 同一個 balance PD: u_norm = -(Kp*theta + Kd*gyro) in [-1,1]
// - per-wheel bias + per-wheel delta_max window
// - gentle kick: per-wheel kick ppm，用雙輪平均 rpm 判斷是否卡住
// - safety: stale encoder/imu + theta_cut_deg

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
#include <stdatomic.h>

#include "/home/william/rpi4_dev/driver/motor/motor_pwm.h"
#include "/home/william/rpi4_dev/driver/encoder/enc_uapi.h"
#include "/home/william/rpi4_dev/driver/imu/imu_uapi.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

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
    if (cpu < 0) return;
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

static void set_other(void) {
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &sp) != 0) {
        perror("pthread_setschedparam(SCHED_OTHER)");
    }
}

static void lock_memory(void) {
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        perror("mlockall");
        fprintf(stderr, "Hint: check ulimit -l and CAP_IPC_LOCK.\n");
        exit(1);
    }
}

// ===================== shared encoder (seq counter snapshot) =====================
typedef struct {
    atomic_uint seq;
    double rpm;
    double rpm_raw;
    int updated;
    uint64_t ok, eagain, err, shortread;
    uint64_t ts_ns;
} shared_enc_t;

static inline void shared_enc_write(shared_enc_t *sh,
                                    double rpm, double rpm_raw, int updated,
                                    uint64_t ok, uint64_t eagain, uint64_t err, uint64_t shortread,
                                    uint64_t ts_ns)
{
    unsigned s = atomic_load_explicit(&sh->seq, memory_order_relaxed);
    atomic_store_explicit(&sh->seq, s + 1, memory_order_release);

    sh->rpm = rpm;
    sh->rpm_raw = rpm_raw;
    sh->updated = updated;
    sh->ok = ok; sh->eagain = eagain; sh->err = err; sh->shortread = shortread;
    sh->ts_ns = ts_ns;

    atomic_store_explicit(&sh->seq, s + 2, memory_order_release);
}

static inline int shared_enc_read(shared_enc_t *sh,
                                  double *rpm, double *rpm_raw, int *updated,
                                  uint64_t *ok, uint64_t *eagain, uint64_t *err, uint64_t *shortread,
                                  uint64_t *ts_ns)
{
    unsigned s1 = atomic_load_explicit(&sh->seq, memory_order_acquire);
    if (s1 & 1) return 0;

    double r = sh->rpm;
    double rr = sh->rpm_raw;
    int up = sh->updated;
    uint64_t a=sh->ok, b=sh->eagain, c=sh->err, d=sh->shortread, t=sh->ts_ns;

    unsigned s2 = atomic_load_explicit(&sh->seq, memory_order_acquire);
    if (s1 != s2 || (s2 & 1)) return 0;

    *rpm = r; *rpm_raw = rr; *updated = up;
    *ok = a; *eagain = b; *err = c; *shortread = d;
    *ts_ns = t;
    return 1;
}

// ===================== shared IMU (seq counter snapshot) =====================
typedef struct {
    atomic_uint seq;
    double theta;   // rad
    double gyro;    // rad/s
    int updated;
    uint64_t ok, eagain, err, shortread;
    uint64_t ts_ns;
} shared_imu_t;

static inline void shared_imu_write(shared_imu_t *sh,
                                    double theta, double gyro, int updated,
                                    uint64_t ok, uint64_t eagain, uint64_t err, uint64_t shortread,
                                    uint64_t ts_ns)
{
    unsigned s = atomic_load_explicit(&sh->seq, memory_order_relaxed);
    atomic_store_explicit(&sh->seq, s + 1, memory_order_release);

    sh->theta = theta;
    sh->gyro  = gyro;
    sh->updated = updated;
    sh->ok = ok; sh->eagain = eagain; sh->err = err; sh->shortread = shortread;
    sh->ts_ns = ts_ns;

    atomic_store_explicit(&sh->seq, s + 2, memory_order_release);
}

static inline int shared_imu_read(shared_imu_t *sh,
                                  double *theta, double *gyro, int *updated,
                                  uint64_t *ok, uint64_t *eagain, uint64_t *err, uint64_t *shortread,
                                  uint64_t *ts_ns)
{
    unsigned s1 = atomic_load_explicit(&sh->seq, memory_order_acquire);
    if (s1 & 1) return 0;

    double th = sh->theta;
    double gr = sh->gyro;
    int up = sh->updated;
    uint64_t a=sh->ok, b=sh->eagain, c=sh->err, d=sh->shortread, t=sh->ts_ns;

    unsigned s2 = atomic_load_explicit(&sh->seq, memory_order_acquire);
    if (s1 != s2 || (s2 & 1)) return 0;

    *theta = th; *gyro = gr; *updated = up;
    *ok = a; *eagain = b; *err = c; *shortread = d;
    *ts_ns = t;
    return 1;
}

// ===================== RT stats =====================
typedef struct {
    atomic_ulong miss;
    atomic_ulong wakeups;
    atomic_ulong jitter_max_ns;
    atomic_long  jitter_sum_ns;
} rt_stats_t;

// ===================== config =====================
typedef struct {
    int hz;
    int cpu_ctrl;
    int cpu_sens;
    int fifo_prio;

    double rpm0, rpm1;
    int pre_ms, hold_ms;
    const char *csv_path;

    int enc_decim;

    int mode;           // 0 speed, 1 balance

    int imu_axis;
    int imu_sign;
    double alpha;

    // balance PD
    double bal_kp;
    double bal_kd;
    double theta_cut_deg;

    int umax_ppm;

    // per-wheel bias & delta_max
    int bias_L_ppm;
    int bias_R_ppm;
    int delta_max_L_ppm;
    int delta_max_R_ppm;

    // gentle kick
    int kick_ms;
    int kick_ppm_L;
    int kick_ppm_R;
    int kick_cool_ms;
    double kick_rpm_th;
} cfg_t;

typedef struct {
    cfg_t cfg;
    volatile int stop;

    shared_enc_t sh_enc_l;
    shared_enc_t sh_enc_r;
    shared_imu_t sh_imu;

    atomic_int last_u_cmd;

    rt_stats_t rt;
    atomic_ulong age_enc_l_max_ns;
    atomic_ulong age_enc_r_max_ns;
    atomic_ulong age_imu_max_ns;
} ctx_t;

// ===================== encoder state (sensor thread only) =====================
typedef struct {
    int fd;
    struct enc_sample prev;

    double rpm;
    double rpm_raw;

    uint64_t ok, eagain, err, shortread;

    int32_t acc_dcount;
    int64_t acc_dts_ns;

    int decim;
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

static int enc_update(enc_state_t *e) {
    struct enc_sample cur;
    ssize_t n = read(e->fd, &cur, sizeof(cur));

    if (n == (ssize_t)sizeof(cur)) {
        int32_t dcount = cur.count - e->prev.count;
        int64_t dts    = (int64_t)cur.ts_ns - (int64_t)e->prev.ts_ns;
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

            double alpha = 1.0;
            e->rpm = e->rpm + alpha * (rpm_raw - e->rpm);

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

// ===================== sensor thread =====================
static void *sensor_thread(void *arg) {
    ctx_t *c = (ctx_t*)arg;

    bind_cpu(c->cfg.cpu_sens);
    set_other();

    enc_state_t enc_l, enc_r;
    if (enc_open(&enc_l, "/dev/encoder-left", c->cfg.enc_decim) < 0) return NULL;
    if (enc_open(&enc_r, "/dev/encoder-right", c->cfg.enc_decim) < 0) {
        close(enc_l.fd);
        return NULL;
    }

    int imu_fd = open("/dev/imu0", O_RDONLY | O_NONBLOCK);
    if (imu_fd < 0) perror("open /dev/imu0");

    double theta = 0.0;
    uint64_t imu_ok=0, imu_eagain=0, imu_err=0, imu_short=0;
    uint64_t last_imu_ts = 0;

    shared_enc_write(&c->sh_enc_l, enc_l.rpm, enc_l.rpm_raw, 0,
                     enc_l.ok, enc_l.eagain, enc_l.err, enc_l.shortread, nsec_now());
    shared_enc_write(&c->sh_enc_r, enc_r.rpm, enc_r.rpm_raw, 0,
                     enc_r.ok, enc_r.eagain, enc_r.err, enc_r.shortread, nsec_now());
    shared_imu_write(&c->sh_imu, theta, 0.0, 0,
                     imu_ok, imu_eagain, imu_err, imu_short, nsec_now());

    while (!c->stop) {
        int enc_up_l = enc_update(&enc_l);
        int enc_up_r = enc_update(&enc_r);

        shared_enc_write(&c->sh_enc_l,
                         enc_l.rpm, enc_l.rpm_raw, enc_up_l,
                         enc_l.ok, enc_l.eagain, enc_l.err, enc_l.shortread,
                         nsec_now());
        shared_enc_write(&c->sh_enc_r,
                         enc_r.rpm, enc_r.rpm_raw, enc_up_r,
                         enc_r.ok, enc_r.eagain, enc_r.err, enc_r.shortread,
                         nsec_now());

        if (imu_fd >= 0) {
            struct imu_sample s;
            ssize_t n = read(imu_fd, &s, sizeof(s));
            if (n == (ssize_t)sizeof(s)) {
                imu_ok++;

                int16_t g_raw = 0;
                int16_t a_raw = 0;
                if (c->cfg.imu_axis == 0) { g_raw = s.gx; a_raw = s.ax; }
                else if (c->cfg.imu_axis == 1) { g_raw = s.gy; a_raw = s.ay; }
                else { g_raw = s.gz; a_raw = s.az; }

                double gyro_dps = ((double)g_raw / 131.0) * (double)c->cfg.imu_sign;
                double gyro = gyro_dps * (M_PI / 180.0);

                double az = (double)s.az;
                double aa = ((double)a_raw) * (double)c->cfg.imu_sign;
                double theta_acc = atan2(aa, az);

                double dt = 0.0;
                if (last_imu_ts > 0 && s.ts_ns > last_imu_ts)
                    dt = (double)(s.ts_ns - last_imu_ts) / 1e9;
                last_imu_ts = s.ts_ns;

                if (dt > 0.0 && dt < 0.05) {
                    double pred = theta + gyro * dt;
                    theta = c->cfg.alpha * pred + (1.0 - c->cfg.alpha) * theta_acc;
                }

                shared_imu_write(&c->sh_imu, theta, gyro, 1,
                                 imu_ok, imu_eagain, imu_err, imu_short,
                                 nsec_now());
            } else if (n < 0 && errno == EAGAIN) {
                imu_eagain++;
            } else if (n < 0) {
                imu_err++;
            } else if (n > 0) {
                imu_short++;
            }
        }

        usleep(1000);
    }

    if (imu_fd >= 0) close(imu_fd);
    close(enc_l.fd);
    close(enc_r.fd);
    return NULL;
}

// ===================== logger thread =====================
static void *logger_thread(void *arg) {
    ctx_t *c = (ctx_t*)arg;
    set_other();

    while (!c->stop) {
        sleep(1);

        double rpm_l=0, rpm_raw_l=0; int up_l=0;
        uint64_t ok_l=0,eagain_l=0,err_l=0,short_l=0,ts_l=0;
        (void)shared_enc_read(&c->sh_enc_l, &rpm_l, &rpm_raw_l, &up_l,
                              &ok_l, &eagain_l, &err_l, &short_l, &ts_l);

        double rpm_r=0, rpm_raw_r=0; int up_r=0;
        uint64_t ok_r=0,eagain_r=0,err_r=0,short_r=0,ts_r=0;
        (void)shared_enc_read(&c->sh_enc_r, &rpm_r, &rpm_raw_r, &up_r,
                              &ok_r, &eagain_r, &err_r, &short_r, &ts_r);

        double theta=0, gyro=0; int imu_up=0;
        uint64_t iok=0, ieagain=0, ierr=0, ishort=0, ts_imu=0;
        (void)shared_imu_read(&c->sh_imu, &theta, &gyro, &imu_up,
                              &iok, &ieagain, &ierr, &ishort, &ts_imu);

        unsigned long wake = atomic_exchange(&c->rt.wakeups, 0);
        unsigned long miss = atomic_exchange(&c->rt.miss, 0);
        long jitter_sum = atomic_exchange(&c->rt.jitter_sum_ns, 0);
        unsigned long jitter_max = atomic_exchange(&c->rt.jitter_max_ns, 0);

        unsigned long age_l_max = atomic_exchange(&c->age_enc_l_max_ns, 0);
        unsigned long age_r_max = atomic_exchange(&c->age_enc_r_max_ns, 0);
        unsigned long age_imu_max = atomic_exchange(&c->age_imu_max_ns, 0);

        double jitter_avg_us = (wake > 0) ? ((double)jitter_sum / (double)wake) / 1000.0 : 0.0;
        double jitter_max_us = (double)jitter_max / 1000.0;

        printf("[1s] L rpm=%.2f raw=%.2f | R rpm=%.2f raw=%.2f | u(avg)=%d\n",
               rpm_l, rpm_raw_l, rpm_r, rpm_raw_r, atomic_load(&c->last_u_cmd));
        printf("[imu] theta=%.3f rad (%.1f deg) gyro=%.3f rad/s (%.1f dps)\n",
               theta, theta*180.0/M_PI, gyro, gyro*180.0/M_PI);
        printf("[health] wake=%lu miss=%lu jitter_avg=%.1fus jitter_max=%.1fus age_L=%.2fms age_R=%.2fms age_imu=%.2fms\n",
               wake, miss, jitter_avg_us, jitter_max_us,
               (double)age_l_max/1e6, (double)age_r_max/1e6, (double)age_imu_max/1e6);

        fflush(stdout);
    }
    return NULL;
}

// ===================== clamp =====================
static inline int clamp_i(int x, int lo, int hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// ===================== control thread =====================
static void *ctrl_thread(void *arg) {
    ctx_t *c = (ctx_t*)arg;

    bind_cpu(c->cfg.cpu_ctrl);
    set_fifo(c->cfg.fifo_prio);
    lock_memory();

    int motor_fd = open("/dev/motor0", O_RDWR);
    if (motor_fd < 0) { perror("open /dev/motor0"); return NULL; }

    uint32_t en = 1;
    if (ioctl(motor_fd, MOTOR_IOC_ENABLE, &en) < 0) perror("MOTOR_IOC_ENABLE");

    const uint64_t period_ns = 1000000000ULL / (uint64_t)c->cfg.hz;

    FILE *fp = fopen(c->cfg.csv_path, "w");
    if (!fp) { perror("fopen csv"); close(motor_fd); return NULL; }

    fprintf(fp,
            "t_ms,mode,rpm_ref,"
            "rpm_l,rpm_raw_l,enc_up_l,age_l_ms,"
            "rpm_r,rpm_raw_r,enc_up_r,age_r_ms,"
            "theta,gyro,imu_up,age_imu_ms,"
            "uL,uR,deltaL,deltaR,kick\n");
    fflush(fp);

    struct motor_cmd mc = {
        .left = 0, .right = 0,
        .flags = MOTOR_FLAG_ENABLE,
        .reserved = 0,
    };

    const uint64_t STALE_ENC_NS = 50ULL * 1000 * 1000;
    const uint64_t STALE_IMU_NS = 50ULL * 1000 * 1000;

    uint64_t t0 = nsec_now();
    uint64_t end_ns = t0 + (uint64_t)(c->cfg.pre_ms + c->cfg.hold_ms) * 1000000ULL;

    uint64_t next_ns = t0 + period_ns;
    int flush_k = 0;

    // kick state (ticks)
    int kick_left_ticks = 0;
    int cool_left_ticks = 0;
    int kick_flag = 0;

    while (!c->stop) {
        struct timespec ts_sleep;
        ts_from_ns(next_ns, &ts_sleep);

        int rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts_sleep, NULL);
        if (rc != 0 && rc != EINTR) { errno = rc; perror("clock_nanosleep"); }

        uint64_t now_ns = nsec_now();

        int64_t jitter_ns = (int64_t)(now_ns - next_ns);
        if (jitter_ns < 0) jitter_ns = 0;

        atomic_fetch_add(&c->rt.wakeups, 1);
        atomic_fetch_add(&c->rt.jitter_sum_ns, jitter_ns);

        unsigned long old = atomic_load(&c->rt.jitter_max_ns);
        while ((unsigned long)jitter_ns > old &&
               !atomic_compare_exchange_weak(&c->rt.jitter_max_ns, &old, (unsigned long)jitter_ns)) {}

        if (now_ns > next_ns + period_ns) atomic_fetch_add(&c->rt.miss, 1);

        double t_ms = (double)(now_ns - t0) / 1e6;
        double rpm_ref = (t_ms < (double)c->cfg.pre_ms) ? c->cfg.rpm0 : c->cfg.rpm1;

        // enc L
        double rpm_l=0.0, rpm_raw_l=0.0;
        int enc_up_l=0;
        uint64_t ok_l=0,eagain_l=0,err_l=0,short_l=0,ts_l=0;
        (void)shared_enc_read(&c->sh_enc_l, &rpm_l, &rpm_raw_l, &enc_up_l,
                              &ok_l, &eagain_l, &err_l, &short_l, &ts_l);

        // enc R
        double rpm_r=0.0, rpm_raw_r=0.0;
        int enc_up_r=0;
        uint64_t ok_r=0,eagain_r=0,err_r=0,short_r=0,ts_r=0;
        (void)shared_enc_read(&c->sh_enc_r, &rpm_r, &rpm_raw_r, &enc_up_r,
                              &ok_r, &eagain_r, &err_r, &short_r, &ts_r);

        // imu
        double theta=0.0, gyro=0.0;
        int imu_up=0;
        uint64_t iok=0,ieagain=0,ierr=0,ishort=0,ts_imu=0;
        (void)shared_imu_read(&c->sh_imu, &theta, &gyro, &imu_up,
                              &iok, &ieagain, &ierr, &ishort, &ts_imu);

        uint64_t age_l_ns   = (ts_l   > 0 && now_ns > ts_l)   ? (now_ns - ts_l)   : 0;
        uint64_t age_r_ns   = (ts_r   > 0 && now_ns > ts_r)   ? (now_ns - ts_r)   : 0;
        uint64_t age_imu_ns = (ts_imu > 0 && now_ns > ts_imu) ? (now_ns - ts_imu) : 0;

        double age_l_ms   = (double)age_l_ns   / 1e6;
        double age_r_ms   = (double)age_r_ns   / 1e6;
        double age_imu_ms = (double)age_imu_ns / 1e6;

        // max age stats
        unsigned long old_age = atomic_load(&c->age_enc_l_max_ns);
        while ((unsigned long)age_l_ns > old_age &&
               !atomic_compare_exchange_weak(&c->age_enc_l_max_ns, &old_age, (unsigned long)age_l_ns)) {}
        old_age = atomic_load(&c->age_enc_r_max_ns);
        while ((unsigned long)age_r_ns > old_age &&
               !atomic_compare_exchange_weak(&c->age_enc_r_max_ns, &old_age, (unsigned long)age_r_ns)) {}
        old_age = atomic_load(&c->age_imu_max_ns);
        while ((unsigned long)age_imu_ns > old_age &&
               !atomic_compare_exchange_weak(&c->age_imu_max_ns, &old_age, (unsigned long)age_imu_ns)) {}

        // safety
        bool stale = false;
        if (age_l_ns > STALE_ENC_NS) stale = true;
        if (age_r_ns > STALE_ENC_NS) stale = true;
        if (c->cfg.mode == 1 && age_imu_ns > STALE_IMU_NS) stale = true;

        if (c->cfg.mode == 1) {
            double theta_deg = theta * 180.0 / M_PI;
            if (fabs(theta_deg) > c->cfg.theta_cut_deg) stale = true;
        }

        int uL = 0, uR = 0;
        int deltaL = 0, deltaR = 0;

        if (stale) {
            kick_left_ticks = 0;
            cool_left_ticks = 0;
            kick_flag = 0;

            atomic_store(&c->last_u_cmd, 0);

            mc.left = 0; mc.right = 0;
            (void)ioctl(motor_fd, MOTOR_IOC_SET, &mc);

            fprintf(fp,
                    "%.3f,%d,%.3f,"
                    "%.3f,%.3f,%d,%.3f,"
                    "%.3f,%.3f,%d,%.3f,"
                    "%.6f,%.6f,%d,%.3f,"
                    "%d,%d,%d,%d,%d\n",
                    t_ms, c->cfg.mode, rpm_ref,
                    rpm_l, rpm_raw_l, enc_up_l, age_l_ms,
                    rpm_r, rpm_raw_r, enc_up_r, age_r_ms,
                    theta, gyro, imu_up, age_imu_ms,
                    0, 0, 0, 0, 0);

            if (++flush_k >= 50) { fflush(fp); flush_k = 0; }
            next_ns += period_ns;
            continue;
        }

        if (c->cfg.mode == 0) {
            // speed mode not used now
            uL = 0; uR = 0;
            deltaL = 0; deltaR = 0;
            kick_left_ticks = 0;
            cool_left_ticks = 0;
            kick_flag = 0;
        } else {
            // ================= balance mode (no latch/slew) =================
            double u_norm = -(c->cfg.bal_kp * theta + c->cfg.bal_kd * gyro);
            if (u_norm >  1.0) u_norm =  1.0;
            if (u_norm < -1.0) u_norm = -1.0;

            int sgn = (u_norm >= 0.0) ? 1 : -1;
            double mag = fabs(u_norm);

	    int dL = (int)(mag * (double)c->cfg.delta_max_L_ppm + 0.5);
	    int dR = (int)(mag * (double)c->cfg.delta_max_R_ppm + 0.5);
	    dL = clamp_i(dL, 0, c->cfg.delta_max_L_ppm);
	    dR = clamp_i(dR, 0, c->cfg.delta_max_R_ppm);

	    // delta is signed already (sgn * d)
	    deltaL = sgn * dL;
	    deltaR = sgn * dR;

	    // bias follows direction
	    int biasL = sgn * c->cfg.bias_L_ppm;
	    int biasR = sgn * c->cfg.bias_R_ppm;

	    uL = biasL + deltaL;
	    uR = biasR + deltaR;

	    uL = clamp_i(uL, -c->cfg.umax_ppm, c->cfg.umax_ppm);
	    uR = clamp_i(uR, -c->cfg.umax_ppm, c->cfg.umax_ppm);
	    // ================= gentle kick =================
            double rpm_avg = 0.5 * (rpm_l + rpm_r);
            bool low_rpm = (fabs(rpm_avg) < c->cfg.kick_rpm_th);

            int tick_ms = (int)(1000.0 / (double)c->cfg.hz + 0.5);
            if (tick_ms <= 0) tick_ms = 5;

            if (cool_left_ticks > 0) cool_left_ticks--;
            if (kick_left_ticks > 0) kick_left_ticks--;
            kick_flag = 0;

            if (kick_left_ticks <= 0 && cool_left_ticks <= 0 && low_rpm) {
                kick_left_ticks = (c->cfg.kick_ms + tick_ms - 1) / tick_ms;
                cool_left_ticks = (c->cfg.kick_cool_ms + tick_ms - 1) / tick_ms;
            }

            if (kick_left_ticks > 0) {
                int kL = c->cfg.kick_ppm_L; if (kL < 0) kL = -kL;
                int kR = c->cfg.kick_ppm_R; if (kR < 0) kR = -kR;
                kL = clamp_i(kL, 0, c->cfg.umax_ppm);
                kR = clamp_i(kR, 0, c->cfg.umax_ppm);

                uL = sgn * kL;
                uR = sgn * kR;
                kick_flag = 1;
            }
        }

        atomic_store(&c->last_u_cmd, (uL + uR) / 2);

        mc.left  = uL;
        mc.right = uR;
        if (ioctl(motor_fd, MOTOR_IOC_SET, &mc) < 0) perror("MOTOR_IOC_SET");

        fprintf(fp,
                "%.3f,%d,%.3f,"
                "%.3f,%.3f,%d,%.3f,"
                "%.3f,%.3f,%d,%.3f,"
                "%.6f,%.6f,%d,%.3f,"
                "%d,%d,%d,%d,%d\n",
                t_ms, c->cfg.mode, rpm_ref,
                rpm_l, rpm_raw_l, enc_up_l, age_l_ms,
                rpm_r, rpm_raw_r, enc_up_r, age_r_ms,
                theta, gyro, imu_up, age_imu_ms,
                uL, uR, deltaL, deltaR, kick_flag);

        if (++flush_k >= 50) { fflush(fp); flush_k = 0; }

        if (now_ns >= end_ns) { fflush(fp); break; }

        next_ns += period_ns;
        uint64_t n2 = nsec_now();
        if (n2 > next_ns + period_ns) next_ns = n2 + period_ns;
    }

    mc.left = 0; mc.right = 0;
    (void)ioctl(motor_fd, MOTOR_IOC_SET, &mc);

    fclose(fp);
    close(motor_fd);
    return NULL;
}

// ===================== main =====================
int main(int argc, char **argv) {
    ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    // defaults
    ctx.cfg.hz = 200;
    ctx.cfg.cpu_ctrl = 3;
    ctx.cfg.cpu_sens = 2;
    ctx.cfg.fifo_prio = 80;

    ctx.cfg.rpm0 = 0.0;
    ctx.cfg.rpm1 = 0.0;
    ctx.cfg.pre_ms = 1000;
    ctx.cfg.hold_ms = 6000;
    ctx.cfg.csv_path = "run_bal.csv";

    ctx.cfg.enc_decim = 4;
    ctx.cfg.mode = 1;

    ctx.cfg.imu_axis = 1;
    ctx.cfg.imu_sign = 1;
    ctx.cfg.alpha = 0.98;

    ctx.cfg.bal_kp = 1.2;
    ctx.cfg.bal_kd = 0.06;
    ctx.cfg.theta_cut_deg = 30.0;

    ctx.cfg.umax_ppm = 1000000;

    ctx.cfg.bias_L_ppm = 559379;
    ctx.cfg.bias_R_ppm = 559379;
    ctx.cfg.delta_max_L_ppm = 200000;
    ctx.cfg.delta_max_R_ppm = 200000;

    ctx.cfg.kick_ms = 15;
    ctx.cfg.kick_ppm_L = 680000;
    ctx.cfg.kick_ppm_R = 680000;
    ctx.cfg.kick_cool_ms = 700;
    ctx.cfg.kick_rpm_th = 2.0;

    // usage:
    // balancertd_arch [hz] [cpu_ctrl] [prio] [rpm0] [rpm1] [pre_ms] [hold_ms] [csv] [decim] [cpu_sens]
    //               [mode] [imu_axis] [imu_sign] [alpha] [Kp] [Kd] [theta_cut_deg]
    //               [umax_ppm]
    //               [bias_L] [bias_R]
    //               [delta_max_L] [delta_max_R]
    //               [kick_ms] [kick_ppm_L] [kick_ppm_R] [kick_cool_ms] [kick_rpm_th]
    if (argc > 1)  ctx.cfg.hz = atoi(argv[1]);
    if (argc > 2)  ctx.cfg.cpu_ctrl = atoi(argv[2]);
    if (argc > 3)  ctx.cfg.fifo_prio = atoi(argv[3]);
    if (argc > 4)  ctx.cfg.rpm0 = atof(argv[4]);
    if (argc > 5)  ctx.cfg.rpm1 = atof(argv[5]);
    if (argc > 6)  ctx.cfg.pre_ms = atoi(argv[6]);
    if (argc > 7)  ctx.cfg.hold_ms = atoi(argv[7]);
    if (argc > 8)  ctx.cfg.csv_path = argv[8];
    if (argc > 9)  ctx.cfg.enc_decim = atoi(argv[9]);
    if (argc > 10) ctx.cfg.cpu_sens = atoi(argv[10]);

    if (argc > 11) ctx.cfg.mode = atoi(argv[11]);
    if (argc > 12) ctx.cfg.imu_axis = atoi(argv[12]);
    if (argc > 13) ctx.cfg.imu_sign = atoi(argv[13]);
    if (argc > 14) ctx.cfg.alpha = atof(argv[14]);
    if (argc > 15) ctx.cfg.bal_kp = atof(argv[15]);
    if (argc > 16) ctx.cfg.bal_kd = atof(argv[16]);
    if (argc > 17) ctx.cfg.theta_cut_deg = atof(argv[17]);

    if (argc > 18) ctx.cfg.umax_ppm = atoi(argv[18]);

    if (argc > 19) ctx.cfg.bias_L_ppm = atoi(argv[19]);
    if (argc > 20) ctx.cfg.bias_R_ppm = atoi(argv[20]);

    if (argc > 21) ctx.cfg.delta_max_L_ppm = atoi(argv[21]);
    if (argc > 22) ctx.cfg.delta_max_R_ppm = atoi(argv[22]);

    if (argc > 23) ctx.cfg.kick_ms = atoi(argv[23]);
    if (argc > 24) ctx.cfg.kick_ppm_L = atoi(argv[24]);
    if (argc > 25) ctx.cfg.kick_ppm_R = atoi(argv[25]);
    if (argc > 26) ctx.cfg.kick_cool_ms = atoi(argv[26]);
    if (argc > 27) ctx.cfg.kick_rpm_th = atof(argv[27]);

    printf("balancertd_arch: hz=%d ctrl_cpu=%d sens_cpu=%d prio=%d | pre=%dms hold=%dms | csv=%s | decim=%d | mode=%d | imu_axis=%d imu_sign=%d alpha=%.3f\n",
           ctx.cfg.hz, ctx.cfg.cpu_ctrl, ctx.cfg.cpu_sens, ctx.cfg.fifo_prio,
           ctx.cfg.pre_ms, ctx.cfg.hold_ms,
           ctx.cfg.csv_path, ctx.cfg.enc_decim, ctx.cfg.mode,
           ctx.cfg.imu_axis, ctx.cfg.imu_sign, ctx.cfg.alpha);

    if (ctx.cfg.mode == 1) {
        printf("BAL: Kp=%.3f Kd=%.3f cut=%.1fdeg | umax=%d | biasL=%d biasR=%d | dmaxL=%d dmaxR=%d | kick %dms L=%d R=%d cool=%dms th=%.2f\n",
               ctx.cfg.bal_kp, ctx.cfg.bal_kd, ctx.cfg.theta_cut_deg,
               ctx.cfg.umax_ppm,
               ctx.cfg.bias_L_ppm, ctx.cfg.bias_R_ppm,
               ctx.cfg.delta_max_L_ppm, ctx.cfg.delta_max_R_ppm,
               ctx.cfg.kick_ms, ctx.cfg.kick_ppm_L, ctx.cfg.kick_ppm_R,
               ctx.cfg.kick_cool_ms, ctx.cfg.kick_rpm_th);
    }

    pthread_t th_ctrl, th_sens, th_log;

    if (pthread_create(&th_sens, NULL, sensor_thread, &ctx) != 0) {
        perror("pthread_create sensor"); return 1;
    }
    if (pthread_create(&th_log, NULL, logger_thread, &ctx) != 0) {
        perror("pthread_create logger"); return 1;
    }
    if (pthread_create(&th_ctrl, NULL, ctrl_thread, &ctx) != 0) {
        perror("pthread_create ctrl"); return 1;
    }

    int total_ms = ctx.cfg.pre_ms + ctx.cfg.hold_ms + 800;
    usleep(total_ms * 1000);

    ctx.stop = 1;

    pthread_join(th_ctrl, NULL);
    pthread_join(th_sens, NULL);
    pthread_join(th_log, NULL);

    return 0;
}
