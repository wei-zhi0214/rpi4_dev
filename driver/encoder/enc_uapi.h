#ifndef _ENC_UAPI_H_
#define _ENC_UAPI_H_

#ifdef __KERNEL__
  #include <linux/types.h>
#else
  #include <stdint.h>
#endif

struct enc_sample {
#ifdef __KERNEL__
    s64 ts_ns;
    s32 count;
#else
    int64_t ts_ns;
    int32_t count;
#endif
};

#endif

