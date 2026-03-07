// motor_step_test.c (C version, gcc -static ok)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include "/home/william/rpi4_dev/driver/motor/motor_pwm.h"
#include "/home/william/rpi4_dev/driver/encoder/enc_uapi.h"


#ifndef COUNTS_PER_REV
#define COUNTS_PER_REV 1320.0
#endif

static int xioctl(int fd, unsigned long req, void *arg)
{
    int r = ioctl(fd, req, arg);
    if (r < 0) fprintf(stderr, "ioctl 0x%lx failed: %s\n", req, strerror(errno));
    return r;
}

static int64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void motor_set_u(int fm, int u)
{
    struct motor_cmd cmd;
    cmd.left = u;
    cmd.right = u;
    cmd.flags = MOTOR_FLAG_ENABLE;
    cmd.reserved = 0;
    xioctl(fm, MOTOR_IOC_SET, &cmd);
}

int main(int argc, char **argv)
{
    if (argc < 6) {
        fprintf(stderr,
            "Usage: %s <motor_dev> <enc_dev> <u_step> <hold_ms> <sample_ms> [csv_path]\n"
            "Example: %s /dev/motor0 /dev/encoder-left 300 4000 10 out.csv\n",
            argv[0], argv[0]);
        return 1;
    }

    const char *motor_dev = argv[1];
    const char *enc_dev   = argv[2];
    int u_step             = atoi(argv[3]);     // e.g. 200..800
    int hold_ms            = atoi(argv[4]);     // e.g. 4000
    int sample_ms          = atoi(argv[5]);     // e.g. 5/10/20
    const char *csv_path   = (argc >= 7) ? argv[6] : "motor_step.csv";

    int fm = open(motor_dev, O_RDWR);
    if (fm < 0) { perror("open motor"); return 1; }

    int fe = open(enc_dev, O_RDONLY);
    if (fe < 0) { perror("open encoder"); return 1; }

    FILE *fp = fopen(csv_path, "w");
    if (!fp) { perror("fopen csv"); return 1; }

    fprintf(fp, "t_ms,u,count,dcount,dt_ms,rpm\n");
    fflush(fp);

    int en = 1;
    if (xioctl(fm, MOTOR_IOC_ENABLE, &en) < 0) return 1;

    const int pre_ms  = 1000;
    const int post_ms = 1000;
    const int total_ms = pre_ms + hold_ms + post_ms;

    struct enc_sample s_prev, s_now;
    if (read(fe, &s_prev, sizeof(s_prev)) != (ssize_t)sizeof(s_prev)) {
        fprintf(stderr, "read encoder failed\n");
        return 1;
    }

    int64_t t0 = now_ns();
    int64_t last_ns = t0;

    for (;;) {
        int64_t t_ns = now_ns();
        int t_ms = (int)((t_ns - t0) / 1000000LL);
        if (t_ms > total_ms) break;

        int u = 0;
        if (t_ms >= pre_ms && t_ms < pre_ms + hold_ms) u = u_step;
        motor_set_u(fm, u);

        if (read(fe, &s_now, sizeof(s_now)) != (ssize_t)sizeof(s_now)) {
            fprintf(stderr, "read encoder failed: %s\n", strerror(errno));
            break;
        }

        int32_t dcount = s_now.count - s_prev.count;
        double dt_ms = (double)(t_ns - last_ns) / 1e6;
        double dt_s  = (double)(t_ns - last_ns) / 1e9;

        double rpm = 0.0;
        if (dt_s > 0.0) {
            double rev_s = ((double)dcount) / (COUNTS_PER_REV * dt_s);
            rpm = 60.0 * rev_s;
        }

        fprintf(fp, "%d,%d,%d,%d,%.4f,%.3f\n",
                t_ms, u, s_now.count, dcount, dt_ms, rpm);

        s_prev = s_now;
        last_ns = t_ns;

        usleep(sample_ms * 1000);
    }

    xioctl(fm, MOTOR_IOC_BRAKE, NULL);
    en = 0;
    xioctl(fm, MOTOR_IOC_ENABLE, &en);

    fclose(fp);
    close(fe);
    close(fm);

    printf("CSV saved: %s\n", csv_path);
    return 0;
}

