#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include "imu_uapi.h"

int main(void)
{
    int fd = open("/dev/imu0", O_RDONLY);
    if (fd < 0) { perror("open /dev/imu0"); return 1; }

    struct pollfd p = { .fd = fd, .events = POLLIN };

    while (1) {
        int pr = poll(&p, 1, 1000);
        if (pr < 0) { perror("poll"); break; }
        if (pr == 0) { puts("poll timeout"); continue; }

        struct imu_sample s;
        ssize_t n = read(fd, &s, sizeof(s));
        if (n < 0) { perror("read"); break; }
        if (n != sizeof(s)) { printf("short read: %zd\n", n); continue; }

        printf("ts=%lld ax=%d ay=%d az=%d gx=%d gy=%d gz=%d\n",
               (long long)s.ts_ns, s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
    }

    close(fd);
    return 0;
}

