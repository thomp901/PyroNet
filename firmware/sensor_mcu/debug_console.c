#include "debug_console.h"

#include <stddef.h>
#include <sys/types.h>

#include "em_device.h"
#include "sl_clock_manager.h"
#include "sl_clock_manager_tree_config.h"

#define DEBUG_CONSOLE_ITM_PORT              0U
#define DEBUG_CONSOLE_SYNC_ITM_PORT         8U
#define DEBUG_CONSOLE_TARGET_SWO_SPEED_HZ   875000U
#define DEBUG_CONSOLE_ITM_READY_TIMEOUT     1048575U

static uint32_t debug_console_configured_swo_speed_hz = 0U;

static void debug_console_configure_swo_pin(void)
{
  const uint32_t shift = GPIO_SWV_PIN * 4U;

  GPIO->P[GPIO_SWV_PORT].MODEL =
    (GPIO->P[GPIO_SWV_PORT].MODEL & ~((uint32_t)_GPIO_P_MODEL_MODE0_MASK << shift))
    | ((uint32_t)_GPIO_P_MODEL_MODE0_PUSHPULL << shift);

  GPIO->TRACEROUTEPEN_CLR = GPIO_TRACEROUTEPEN_TRACECLKPEN
                            | GPIO_TRACEROUTEPEN_TRACEDATA0PEN
                            | GPIO_TRACEROUTEPEN_TRACEDATA1PEN
                            | GPIO_TRACEROUTEPEN_TRACEDATA2PEN
                            | GPIO_TRACEROUTEPEN_TRACEDATA3PEN;
  GPIO->TRACEROUTEPEN_SET = GPIO_TRACEROUTEPEN_SWVPEN;
}

static void debug_console_configure_trace_clock(void)
{
  uint32_t traceclk = CMU->TRACECLKCTRL;

  traceclk &= ~(_CMU_TRACECLKCTRL_CLKSEL_MASK | _CMU_TRACECLKCTRL_PRESC_MASK);
  traceclk |= SL_CLOCK_MANAGER_TRACECLK_SOURCE | SL_CLOCK_MANAGER_TRACECLK_DIVIDER;

  CMU->TRACECLKCTRL = traceclk;
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

static void debug_console_putc(char ch)
{
  uint32_t timeout = DEBUG_CONSOLE_ITM_READY_TIMEOUT;

  if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U) {
    return;
  }
  if ((ITM->TCR & ITM_TCR_ITMENA_Msk) == 0U) {
    return;
  }
  do {
    // Some J-Link based probes drop trace enable state across reconnects.
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
  uint32_t divider;

  CMU->CLKEN0_SET = CMU_CLKEN0_GPIO;
  debug_console_configure_swo_pin();
  debug_console_configure_trace_clock();

  trace_clock_hz = debug_console_trace_clock_hz();
  divider = (trace_clock_hz + (DEBUG_CONSOLE_TARGET_SWO_SPEED_HZ / 2U))
            / DEBUG_CONSOLE_TARGET_SWO_SPEED_HZ;
  if (divider == 0U) {
    divider = 1U;
  }
  debug_console_configured_swo_speed_hz = trace_clock_hz / divider;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL = 0x400003FFUL;
  TPI->ACPR = divider - 1U;
  TPI->SPPR = 2U;
  TPI->FFCR = 0x100U;

  ITM->LAR = 0xC5ACCE55UL;
  ITM->TCR = 0x10009UL;
  ITM->TER = (1UL << DEBUG_CONSOLE_ITM_PORT);

  // Match the SDK SWO driver: send one sync byte on a spare channel so the
  // first application line after re-init is not lost by the probe.
  ITM->TER |= (1UL << DEBUG_CONSOLE_SYNC_ITM_PORT);
  ITM->PORT[DEBUG_CONSOLE_SYNC_ITM_PORT].u8 = 0xFFU;
  ITM->TER &= ~(1UL << DEBUG_CONSOLE_SYNC_ITM_PORT);
}

const char *debug_console_backend_name(void)
{
  return "SWO";
}

uint32_t debug_console_swo_speed_hz(void)
{
  return debug_console_configured_swo_speed_hz;
}

int _write(int file, char *ptr, int len)
{
  (void)file;

  for (int i = 0; i < len; i++) {
    if (ptr[i] == '\n') {
      debug_console_putc('\r');
    }
    debug_console_putc(ptr[i]);
  }

  return len;
}
