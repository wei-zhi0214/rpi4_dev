#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "mpu6050_ioctl.h"
#include <unistd.h>

int main(void)
{
    int fd = open("/dev/mpu6050", O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    struct mpu6050_data data;
    if (ioctl(fd, MPU6050_IOCTL_GET_DATA, &data) < 0) {
        perror("ioctl");
        return -1;
    }

    printf("Accel: X=%d Y=%d Z=%d\n", data.accel_x, data.accel_y, data.accel_z);
    printf("Gyro : X=%d Y=%d Z=%d\n", data.gyro_x,  data.gyro_y,  data.gyro_z);

    close(fd);
    return 0;
}

