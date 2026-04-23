#ifndef PLATFORM_MONOTONIC_TIME_H
#define PLATFORM_MONOTONIC_TIME_H

#include <stdint.h>

void monotonic_time_init(void);
uint64_t monotonic_time_now_cycles(void);
uint64_t monotonic_time_now_us(void);
int64_t monotonic_time_now_ns(void);
void monotonic_time_delay_us(uint32_t period_us);

#endif
