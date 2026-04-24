#include "platform/monotonic_time.h"

#include <stdbool.h>

#include "em_device.h"

#define MONOTONIC_TIME_US_PER_SECOND  1000000ULL
#define MONOTONIC_TIME_NS_PER_SECOND  1000000000ULL

static bool monotonic_time_initialized = false;
static uint32_t monotonic_time_last_cycles = 0U;
static uint64_t monotonic_time_wrap_count = 0U;

static void monotonic_time_enable_cycle_counter(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint64_t monotonic_time_cycles_to_units(uint64_t cycles,
                                               uint64_t units_per_second)
{
  uint64_t seconds;
  uint64_t remaining_cycles;
  uint64_t scaled_seconds;

  if ((SystemCoreClock == 0U) || (units_per_second == 0U)) {
    return 0U;
  }

  seconds = cycles / (uint64_t)SystemCoreClock;
  remaining_cycles = cycles % (uint64_t)SystemCoreClock;

  if (seconds > (UINT64_MAX / units_per_second)) {
    return UINT64_MAX;
  }

  scaled_seconds = seconds * units_per_second;
  return scaled_seconds
         + ((remaining_cycles * units_per_second) / (uint64_t)SystemCoreClock);
}

void monotonic_time_init(void)
{
  monotonic_time_enable_cycle_counter();
  monotonic_time_last_cycles = DWT->CYCCNT;
  monotonic_time_wrap_count = 0U;
  monotonic_time_initialized = true;
}

uint64_t monotonic_time_now_cycles(void)
{
  uint32_t current_cycles;

  if (!monotonic_time_initialized) {
    monotonic_time_init();
  }

  current_cycles = DWT->CYCCNT;
  if (current_cycles < monotonic_time_last_cycles) {
    monotonic_time_wrap_count++;
  }

  monotonic_time_last_cycles = current_cycles;

  return (monotonic_time_wrap_count << 32) | current_cycles;
}

uint64_t monotonic_time_now_us(void)
{
  uint64_t cycles = monotonic_time_now_cycles();

  return monotonic_time_cycles_to_units(cycles, MONOTONIC_TIME_US_PER_SECOND);
}

int64_t monotonic_time_now_ns(void)
{
  uint64_t cycles = monotonic_time_now_cycles();
  uint64_t ns = monotonic_time_cycles_to_units(cycles,
                                               MONOTONIC_TIME_NS_PER_SECOND);

  if (ns > (uint64_t)INT64_MAX) {
    return INT64_MAX;
  }

  return (int64_t)ns;
}

void monotonic_time_delay_us(uint32_t period_us)
{
  uint64_t start_us;

  if (period_us == 0U) {
    return;
  }

  start_us = monotonic_time_now_us();
  while ((monotonic_time_now_us() - start_us) < period_us) {
  }
}
