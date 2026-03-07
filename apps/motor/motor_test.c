#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include "/home/william/rpi4_dev/driver/motor/motor_pwm.h"

static void set_enable(int fd, uint32_t en) {
    if (ioctl(fd, MOTOR_IOC_ENABLE, &en) < 0) perror("MOTOR_IOC_ENABLE");
}

static void set_lr(int fd, int32_t l, int32_t r, uint32_t flags) {
    struct motor_cmd mc = {.left=l, .right=r, .flags=flags, .reserved=0};
    if (ioctl(fd, MOTOR_IOC_SET, &mc) < 0) perror("MOTOR_IOC_SET");
}

static void brake(int fd) {
    if (ioctl(fd, MOTOR_IOC_BRAKE) < 0) perror("MOTOR_IOC_BRAKE");
}

int main() {
    int fd = open("/dev/motor0", O_RDWR);
    if (fd < 0) { perror("open /dev/motor0"); return 1; }

    set_enable(fd, 1);

    // 左右輪 20% duty forward
    set_lr(fd, 200000, 200000, MOTOR_FLAG_ENABLE);
    sleep(1);

    // 左 forward 30%, 右 backward 30%
    set_lr(fd, 300000, -300000, MOTOR_FLAG_ENABLE);
    sleep(1);

    brake(fd);
    close(fd);
    return 0;
}

