#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include "/home/william/rpi4_dev/driver/encoder/enc_uapi.h"

static double ns_to_s(int64_t ns) { return (double)ns / 1e9; }

int main(int argc, char **argv)
{
    const char *dev = (argc >= 2) ? argv[1] : "/dev/encoder-left";

    // JGB37-520: 11 lines, quadrature x4 => 44 counts/motor_rev
    // gear 1:30 => 1320 counts/output_rev
    const double COUNTS_PER_REV_OUT = 1320.0;

    int fd = open(dev, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    struct enc_sample prev = {0}, cur = {0};

    if (read(fd, &prev, sizeof(prev)) != sizeof(prev)) {
        perror("read first");
        return 1;
    }

    while (1) {
        usleep(10 * 1000); // 10ms window

        ssize_t n = read(fd, &cur, sizeof(cur));
        if (n != sizeof(cur)) {
            if (n < 0) fprintf(stderr, "read: %s\n", strerror(errno));
            else fprintf(stderr, "short read: %zd\n", n);
            continue;
        }

        int32_t dcount = cur.count - prev.count;
        int64_t dts = cur.ts_ns - prev.ts_ns;
        if (dts <= 0) { prev = cur; continue; }

        double dt = ns_to_s(dts);
        double rev_per_s = (dcount / COUNTS_PER_REV_OUT) / dt;
        double rpm = rev_per_s * 60.0;

        printf("%s count=%d dcount=%d dt=%.4fms rpm=%.2f\n",
               dev, cur.count, dcount, dt*1000.0, rpm);

        prev = cur;
    }

    close(fd);
    return 0;
}

