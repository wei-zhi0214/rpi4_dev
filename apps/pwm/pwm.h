#pragma once
#include <linux/ioctl.h>

#define PWM_IOC_MAGIC     'p'
#define PWM_IOC_SET_DUTY  _IOW(PWM_IOC_MAGIC, 0, int)   // 0..100
#define PWM_IOC_SET_FREQ  _IOW(PWM_IOC_MAGIC, 1, int)   // Hz (>0)

