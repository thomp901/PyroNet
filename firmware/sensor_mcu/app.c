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

#include "app.h"

#include <stdio.h>

#include "debug_console.h"
#include "em_cmu.h"
#include "em_eusart.h"
#include "em_gpio.h"
#include "sensor_bus.h"

#define APP_UART_PERIPHERAL            EUSART1
#define APP_UART_BAUDRATE              115200U
#define APP_UART_RX_PORT               gpioPortC
#define APP_UART_RX_PIN                0U
#define APP_UART_TX_PORT               gpioPortC
#define APP_UART_TX_PIN                1U

static void app_uart_init(void)
{
  EUSART_UartInit_TypeDef init = EUSART_UART_INIT_DEFAULT_HF;

  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_EUSART1, true);

  GPIO_PinModeSet(APP_UART_TX_PORT, APP_UART_TX_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(APP_UART_RX_PORT, APP_UART_RX_PIN, gpioModeInputPull, 1);

  GPIO->EUSARTROUTE[EUSART_NUM(APP_UART_PERIPHERAL)].TXROUTE =
    (APP_UART_TX_PORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
    | (APP_UART_TX_PIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(APP_UART_PERIPHERAL)].RXROUTE =
    (APP_UART_RX_PORT << _GPIO_EUSART_RXROUTE_PORT_SHIFT)
    | (APP_UART_RX_PIN << _GPIO_EUSART_RXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(APP_UART_PERIPHERAL)].ROUTEEN =
    GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_RXPEN;

  init.enable = eusartDisable;
  init.baudrate = APP_UART_BAUDRATE;
  EUSART_UartInitHf(APP_UART_PERIPHERAL, &init);
  EUSART_Enable(APP_UART_PERIPHERAL, eusartEnable);
}

void app_init_early(void)
{
  debug_console_init();
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  sensor_bus_state_t sensors = sensor_bus_init();

  app_uart_init();

  printf("SENSORS_READY bme68x=%u sps30=%u\r\n",
         sensors.bme68x_present,
         sensors.sps30_present);
  debug_console_emit_boot_markers();
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
}
