#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "pwm.h"

static int set_duty(int fd, int duty) {
    return ioctl(fd, PWM_IOC_SET_DUTY, &duty);
}
static int set_freq(int fd, int hz) {
    return ioctl(fd, PWM_IOC_SET_FREQ, &hz);
}

int main(int argc, char **argv)
{
    int fd = open("/dev/pwm_demo", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    // 1 Hz 50% → 肉眼可見
    if (set_freq(fd, 1)  < 0) perror("ioctl freq");
    if (set_duty(fd, 50) < 0) perror("ioctl duty");
    sleep(2);

    // 1 kHz 75% → 亮一點
    set_freq(fd, 1000);
    set_duty(fd, 75);
    sleep(2);

    // 100% 恆亮
    set_duty(fd, 100);
    close(fd);
    return 0;
}

