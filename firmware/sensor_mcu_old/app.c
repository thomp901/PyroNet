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

#include "bus_bringup.h"
#include "debug_console.h"

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init_early(void)
{
  debug_console_init();
  puts(SWO_SELF_TEST_TOKEN " phase=EARLY");
}

void app_init(void)
{
  board_i2c_init();
  printf("I2C: I2C1 ready scl=PD02 sda=PD03 freq=%lu\n",
         (unsigned long)BOARD_I2C_BITRATE_HZ);

  board_uart_init();
  printf("UART: EUSART1 ready rx=PC0 tx=PC1 baud=%lu\n",
         (unsigned long)BOARD_UART_BAUDRATE);
  board_uart_write("sensor_mcu UART ready\r\n");
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
}
