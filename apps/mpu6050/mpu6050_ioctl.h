#ifndef _MPU6050_IOCTL_H
#define _MPU6050_IOCTL_H

#include <linux/ioctl.h>

#define MPU6050_MAGIC 'M'

struct mpu6050_data {
    int accel_x;
    int accel_y;
    int accel_z;
    int gyro_x;
    int gyro_y;
    int gyro_z;
};

#define MPU6050_IOCTL_GET_DATA _IOR(MPU6050_MAGIC, 0x01, struct mpu6050_data)

#endif

