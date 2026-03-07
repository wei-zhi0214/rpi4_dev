#ifndef _IMU_UAPI_H_
#define _IMU_UAPI_H_

#ifdef __KERNEL__
  #include <linux/types.h>
#else
  #include <stdint.h>
  typedef int64_t  __s64;
  typedef int16_t  __s16;
#endif

struct imu_sample {
    __s64 ts_ns;     // kernel timestamp (ns)
    __s16 ax, ay, az;
    __s16 gx, gy, gz;
};

#endif

