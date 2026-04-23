#include "platform/monotonic_time.h"

#include <stdbool.h>

#include "em_device.h"

static bool monotonic_time_initialized = false;
static uint32_t monotonic_time_last_cycles = 0U;
static uint64_t monotonic_time_wrap_count = 0U;

static void monotonic_time_enable_cycle_counter(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
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

  if (SystemCoreClock == 0U) {
    return 0U;
  }

  return (cycles * 1000000ULL) / (uint64_t)SystemCoreClock;
}

int64_t monotonic_time_now_ns(void)
{
  uint64_t cycles = monotonic_time_now_cycles();

  if (SystemCoreClock == 0U) {
    return 0;
  }

  return (int64_t)((cycles * 1000000000ULL) / (uint64_t)SystemCoreClock);
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
