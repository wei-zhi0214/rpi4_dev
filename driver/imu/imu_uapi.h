#ifndef _IMU_UAPI_H_
#define _IMU_UAPI_H_

#ifdef __KERNEL__
  #include <linux/types.h>
#else
  #include <stdint.h>
#endif

struct imu_sample {
#ifdef __KERNEL__
    s64 ts_ns;
    s16 ax, ay, az;
    s16 gx, gy, gz;
#else
    int64_t ts_ns;
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
#endif
};

#endif

