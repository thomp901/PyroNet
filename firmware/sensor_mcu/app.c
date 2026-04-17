/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include <stdint.h>
#include "em_cmu.h"
#include "em_gpio.h"

enum {
  test_pin_bit0 = 7,
  test_pin_bit1 = 8,
  test_pin_bit2 = 9,
};

static volatile uint32_t systick_ms;
static uint32_t last_update_ms;
static uint8_t test_counter;

static void test_output_binary(uint8_t value)
{
  if (value & 0x1U) {
    GPIO_PinOutSet(gpioPortC, test_pin_bit0);
  } else {
    GPIO_PinOutClear(gpioPortC, test_pin_bit0);
  }

  if (value & 0x2U) {
    GPIO_PinOutSet(gpioPortC, test_pin_bit1);
  } else {
    GPIO_PinOutClear(gpioPortC, test_pin_bit1);
  }

  if (value & 0x4U) {
    GPIO_PinOutSet(gpioPortC, test_pin_bit2);
  } else {
    GPIO_PinOutClear(gpioPortC, test_pin_bit2);
  }
}

static void test_binary_counter_init(void)
{
  GPIO_PinModeSet(gpioPortC, test_pin_bit0, gpioModePushPull, 0);
  GPIO_PinModeSet(gpioPortC, test_pin_bit1, gpioModePushPull, 0);
  GPIO_PinModeSet(gpioPortC, test_pin_bit2, gpioModePushPull, 0);

  test_output_binary(test_counter);

  // Generate a 1 ms timebase so the super loop can update once per second.
  SysTick_Config(CMU_ClockFreqGet(cmuClock_SYSCLK) / 1000U);
}

static void test_binary_counter_process_action(void)
{
  if ((systick_ms - last_update_ms) < 1000U) {
    return;
  }

  last_update_ms = systick_ms;
  test_counter = (test_counter + 1U) & 0x7U;
  test_output_binary(test_counter);
}

void SysTick_Handler(void)
{
  systick_ms++;
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  test_binary_counter_init();
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
  test_binary_counter_process_action();
}
