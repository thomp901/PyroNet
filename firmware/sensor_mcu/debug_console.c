#include "debug_console.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>

#include "em_cmu.h"
#include "em_device.h"
#include "em_gpio.h"
#include "sl_clock_manager.h"
#include "sl_clock_manager_tree_config.h"
#include "sl_hal_gpio.h"

#define DEBUG_CONSOLE_ITM_READY_TIMEOUT        1048575U
#define DEBUG_CONSOLE_BOOT_RETRY_DELAY_MS      100U
#define DEBUG_CONSOLE_TPI_PROTOCOL_NRZ         2U
#define DEBUG_CONSOLE_TRACE_BUS_ID             1U
#define DEBUG_CONSOLE_LOCK_ACCESS_KEY          0xC5ACCE55UL
#define DEBUG_CONSOLE_TPI_FFCR_CONFIG          0x100UL

static uint32_t debug_console_configured_swo_speed_hz = 0U;
static bool debug_console_initialized = false;

static void debug_console_enable_cycle_counter(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t debug_console_ms_to_cycles(uint32_t period_ms)
{
  uint64_t cycles = ((uint64_t)SystemCoreClock * period_ms) / 1000ULL;

  if (cycles == 0ULL) {
    cycles = 1ULL;
  }

  if (cycles > UINT32_MAX) {
    cycles = UINT32_MAX;
  }

  return (uint32_t)cycles;
}

static void debug_console_delay_ms(uint32_t period_ms)
{
  uint32_t start_cycles;
  uint32_t wait_cycles;

  if (period_ms == 0U) {
    return;
  }

  debug_console_enable_cycle_counter();
  wait_cycles = debug_console_ms_to_cycles(period_ms);
  start_cycles = DWT->CYCCNT;

  while ((uint32_t)(DWT->CYCCNT - start_cycles) < wait_cycles) {
  }
}

static uint32_t debug_console_trace_clock_hz(void)
{
  uint32_t trace_clock_hz = 0U;

  if (sl_clock_manager_get_clock_branch_frequency(SL_CLOCK_BRANCH_TRACECLK, &trace_clock_hz)
      == SL_STATUS_OK) {
    return trace_clock_hz;
  }

#if (SL_CLOCK_MANAGER_TRACECLK_SOURCE == CMU_TRACECLKCTRL_CLKSEL_HFRCOEM23)
  return SystemHFRCOEM23ClockGet();
#elif (SL_CLOCK_MANAGER_TRACECLK_SOURCE == CMU_TRACECLKCTRL_CLKSEL_SYSCLK)
  return SystemSYSCLKGet();
#else
  return SystemSYSCLKGet();
#endif
}

static void debug_console_configure_swo_pin(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet((GPIO_Port_TypeDef)GPIO_SWV_PORT, GPIO_SWV_PIN, gpioModePushPull, 0);

  GPIO->TRACEROUTEPEN_CLR = GPIO_TRACEROUTEPEN_TRACECLKPEN
                            | GPIO_TRACEROUTEPEN_TRACEDATA0PEN
                            | GPIO_TRACEROUTEPEN_TRACEDATA1PEN
                            | GPIO_TRACEROUTEPEN_TRACEDATA2PEN
                            | GPIO_TRACEROUTEPEN_TRACEDATA3PEN;
  sl_hal_gpio_enable_debug_swo(true);
}

static void debug_console_configure_trace_clock(void)
{
  uint32_t traceclk = CMU->TRACECLKCTRL;

  traceclk &= ~(_CMU_TRACECLKCTRL_CLKSEL_MASK | _CMU_TRACECLKCTRL_PRESC_MASK);
  traceclk |= SL_CLOCK_MANAGER_TRACECLK_SOURCE | SL_CLOCK_MANAGER_TRACECLK_DIVIDER;

  CMU->TRACECLKCTRL = traceclk;
}

static bool debug_console_trace_ready(void)
{
  return ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U)
         && ((ITM->TCR & ITM_TCR_ITMENA_Msk) != 0U);
}

static void debug_console_configure_tpi(uint32_t trace_clock_hz)
{
  uint32_t divider;

  divider = (trace_clock_hz + (DEBUG_CONSOLE_TARGET_SWO_SPEED_HZ / 2U))
            / DEBUG_CONSOLE_TARGET_SWO_SPEED_HZ;
  if (divider == 0U) {
    divider = 1U;
  }

  debug_console_configured_swo_speed_hz = trace_clock_hz / divider;
  TPI->ACPR = divider - 1U;
  TPI->SPPR = DEBUG_CONSOLE_TPI_PROTOCOL_NRZ;
  TPI->FFCR = DEBUG_CONSOLE_TPI_FFCR_CONFIG;
}

static void debug_console_configure_itm(void)
{
  ITM->LAR = DEBUG_CONSOLE_LOCK_ACCESS_KEY;
  ITM->TPR = ITM_TPR_PRIVMASK_Msk;
  ITM->TCR = (DEBUG_CONSOLE_TRACE_BUS_ID << ITM_TCR_TRACEBUSID_Pos)
             | ITM_TCR_SWOENA_Msk
             | ITM_TCR_DWTENA_Msk
             | ITM_TCR_ITMENA_Msk;
  ITM->TER = (1UL << DEBUG_CONSOLE_ITM_PORT);

  ITM->TER |= (1UL << DEBUG_CONSOLE_SYNC_ITM_PORT);
  ITM->PORT[DEBUG_CONSOLE_SYNC_ITM_PORT].u8 = 0xFFU;
  ITM->TER &= ~(1UL << DEBUG_CONSOLE_SYNC_ITM_PORT);
}

static void debug_console_putc(char ch)
{
  uint32_t timeout = DEBUG_CONSOLE_ITM_READY_TIMEOUT;

  if (!debug_console_trace_ready()) {
    return;
  }

  do {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    ITM->TER |= (1UL << DEBUG_CONSOLE_ITM_PORT);
  } while ((ITM->PORT[DEBUG_CONSOLE_ITM_PORT].u32 == 0UL) && (--timeout > 0U));

  if (timeout == 0U) {
    return;
  }

  ITM->PORT[DEBUG_CONSOLE_ITM_PORT].u8 = (uint8_t)ch;
}

void debug_console_init(void)
{
  uint32_t trace_clock_hz;

  debug_console_configure_swo_pin();
  debug_console_configure_trace_clock();

  trace_clock_hz = debug_console_trace_clock_hz();
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  debug_console_enable_cycle_counter();
  debug_console_configure_tpi(trace_clock_hz);
  debug_console_configure_itm();
  debug_console_initialized = true;
}

bool debug_console_is_initialized(void)
{
  return debug_console_initialized;
}

const char *debug_console_backend_name(void)
{
  return DEBUG_CONSOLE_BACKEND_NAME;
}

uint32_t debug_console_swo_speed_hz(void)
{
  return debug_console_configured_swo_speed_hz;
}

void debug_console_emit_boot_markers(void)
{
  if (!debug_console_initialized) {
    return;
  }

  printf("%s backend=%s port=%lu speed=%lu\r\n",
         DEBUG_CONSOLE_BOOT_BANNER_MARKER,
         debug_console_backend_name(),
         (unsigned long)DEBUG_CONSOLE_ITM_PORT,
         (unsigned long)debug_console_swo_speed_hz());
  debug_console_delay_ms(DEBUG_CONSOLE_BOOT_RETRY_DELAY_MS);
  puts(DEBUG_CONSOLE_BOOT_READY_MARKER);
}

int _write(int file, char *ptr, int len)
{
  int i;

  (void)file;

  for (i = 0; i < len; i++) {
    if (ptr[i] == '\n') {
      debug_console_putc('\r');
    }
    debug_console_putc(ptr[i]);
  }

  return len;
}
