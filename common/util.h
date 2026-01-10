#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>

uint64_t util_now_ms(void);
uint32_t util_rand_u32(void);
int util_snprintf(char *dst, size_t dst_size, const char *fmt, ...);

#endif
