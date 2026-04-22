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
#include <stdlib.h>

#include "debug_console.h"
#include "em_cmu.h"
#include "em_eusart.h"
#include "em_gpio.h"
#include "platform/monotonic_time.h"
#include "sensor_bus.h"
#include "services/air_quality/bsec_service.h"

#define APP_UART_PERIPHERAL            EUSART1
#define APP_UART_BAUDRATE              115200U
#define APP_UART_RX_PORT               gpioPortC
#define APP_UART_RX_PIN                0U
#define APP_UART_TX_PORT               gpioPortC
#define APP_UART_TX_PIN                1U

static bsec_service_t app_bsec_service;
static bool app_bsec_service_enabled = false;

static long app_scale_float(float value, float scale)
{
  float scaled_value = value * scale;

  if (scaled_value >= 0.0f) {
    scaled_value += 0.5f;
  } else {
    scaled_value -= 0.5f;
  }

  return (long)scaled_value;
}

static void app_log_air_quality_reading(const air_quality_reading_t *reading)
{
  long temperature_centi = app_scale_float(reading->temperature_c, 100.0f);
  long humidity_centi = app_scale_float(reading->humidity_percent, 100.0f);
  long bvoc_milli = app_scale_float(reading->breath_voc_equivalent_ppm, 1000.0f);

  printf("AIR_QUALITY temp_c=%ld.%02ld rh_pct=%ld.%02ld bvoc_ppm=%ld.%03ld\r\n",
         temperature_centi / 100L,
         labs(temperature_centi % 100L),
         humidity_centi / 100L,
         labs(humidity_centi % 100L),
         bvoc_milli / 1000L,
         labs(bvoc_milli % 1000L));
}

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
  monotonic_time_init();

  printf("SENSORS_READY bme68x=%u sps30=%u\r\n",
         sensors.bme68x_present,
         sensors.sps30_present);

  if (sensors.bme68x_present) {
    app_bsec_service_enabled = bsec_service_init(&app_bsec_service, SENSOR_BUS_BME68X_ADDRESS);
    printf("BSEC_INIT status=%s bme68x=%s mode=ULP interval_s=300\r\n",
           bsec_service_status_name(app_bsec_service.last_bsec_status),
           bme688_base_status_name(app_bsec_service.last_bme68x_status));
  }

  debug_console_emit_boot_markers();
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
  air_quality_reading_t reading = { 0 };

  if (!app_bsec_service_enabled) {
    return;
  }

  if (!bsec_service_read(&app_bsec_service, &reading)) {
    return;
  }

  app_log_air_quality_reading(&reading);
}
