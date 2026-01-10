#include "util.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

uint64_t util_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

uint32_t util_rand_u32(void) {
  // jednoduchý seed (na serveri aj klientovi stačí)
  static int seeded = 0;
  if (!seeded) {
    seeded = 1;
    srand((unsigned int)(util_now_ms() ^ (uint64_t)getpid()));
  }
  uint32_t a = (uint32_t)(rand() & 0xFFFF);
  uint32_t b = (uint32_t)(rand() & 0xFFFF);
  return (a << 16) ^ b;
}

int util_snprintf(char *dst, size_t dst_size, const char *fmt, ...) {
  if (!dst || dst_size == 0) return -1;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(dst, dst_size, fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= dst_size) {
    // truncated alebo chyba
    return -1;
  }
  return 0;
}
