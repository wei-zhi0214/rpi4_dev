#ifndef _MOTOR_PWM_UAPI_H_
#define _MOTOR_PWM_UAPI_H_

#ifdef __KERNEL__
  #include <linux/types.h>
  #include <linux/ioctl.h>
#else
  #include <stdint.h>
  #include <sys/ioctl.h>
  typedef int32_t  __s32;
  typedef uint32_t __u32;
#endif

#define MOTOR_IOC_MAGIC 'M'

#define MOTOR_FLAG_ENABLE (1u << 0)
#define MOTOR_FLAG_BRAKE  (1u << 1)
#define MOTOR_FLAG_COAST  (1u << 2)

struct motor_cmd {
    __s32 left;   // -1000000..+1000000
    __s32 right;
    __u32 flags;
    __u32 reserved;
};

#define MOTOR_IOC_SET    _IOW(MOTOR_IOC_MAGIC, 1, struct motor_cmd)
#define MOTOR_IOC_BRAKE  _IO(MOTOR_IOC_MAGIC, 2)
#define MOTOR_IOC_ENABLE _IOW(MOTOR_IOC_MAGIC, 3, __u32)
#endif

