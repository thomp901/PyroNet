#include "bus_bringup.h"

#include <stddef.h>

#include "config/pin_config.h"
#include "em_cmu.h"
#include "em_eusart.h"
#include "em_gpio.h"
#include "em_i2c.h"

#define BOARD_UART_ROUTE_INDEX EUSART_NUM(EUSART1)
#define BOARD_UART_RX_PORT     gpioPortC
#define BOARD_UART_RX_PIN      0U
#define BOARD_UART_TX_PORT     gpioPortC
#define BOARD_UART_TX_PIN      1U

void board_i2c_init(void)
{
  I2C_Init_TypeDef init = I2C_INIT_DEFAULT;

  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(SL_I2C_BUS_CLOCK, true);

  GPIO_PinModeSet((GPIO_Port_TypeDef)SL_I2C_BUS_SCL_PORT,
                  SL_I2C_BUS_SCL_PIN,
                  gpioModeWiredAndPullUp,
                  1);
  GPIO_PinModeSet((GPIO_Port_TypeDef)SL_I2C_BUS_SDA_PORT,
                  SL_I2C_BUS_SDA_PIN,
                  gpioModeWiredAndPullUp,
                  1);

  GPIO->I2CROUTE[SL_I2C_BUS_PERIPHERAL_NO].SCLROUTE =
    ((uint32_t)SL_I2C_BUS_SCL_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT)
    | ((uint32_t)SL_I2C_BUS_SCL_PIN << _GPIO_I2C_SCLROUTE_PIN_SHIFT);
  GPIO->I2CROUTE[SL_I2C_BUS_PERIPHERAL_NO].SDAROUTE =
    ((uint32_t)SL_I2C_BUS_SDA_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT)
    | ((uint32_t)SL_I2C_BUS_SDA_PIN << _GPIO_I2C_SDAROUTE_PIN_SHIFT);
  GPIO->I2CROUTE[SL_I2C_BUS_PERIPHERAL_NO].ROUTEEN =
    GPIO_I2C_ROUTEEN_SCLPEN | GPIO_I2C_ROUTEEN_SDAPEN;

  I2C_Reset(SL_I2C_BUS_PERIPHERAL);

  init.freq = BOARD_I2C_BITRATE_HZ;
  I2C_Init(SL_I2C_BUS_PERIPHERAL, &init);
}

void board_uart_init(void)
{
  EUSART_UartInit_TypeDef init = EUSART_UART_INIT_DEFAULT_HF;

  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_EUSART1, true);

  GPIO_PinModeSet(BOARD_UART_TX_PORT, BOARD_UART_TX_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(BOARD_UART_RX_PORT, BOARD_UART_RX_PIN, gpioModeInput, 0);

  GPIO->EUSARTROUTE[BOARD_UART_ROUTE_INDEX].TXROUTE =
    ((uint32_t)BOARD_UART_TX_PORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
    | ((uint32_t)BOARD_UART_TX_PIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[BOARD_UART_ROUTE_INDEX].RXROUTE =
    ((uint32_t)BOARD_UART_RX_PORT << _GPIO_EUSART_RXROUTE_PORT_SHIFT)
    | ((uint32_t)BOARD_UART_RX_PIN << _GPIO_EUSART_RXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[BOARD_UART_ROUTE_INDEX].ROUTEEN =
    GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_RXPEN;

  EUSART_Reset(EUSART1);
  init.baudrate = BOARD_UART_BAUDRATE;
  EUSART_UartInitHf(EUSART1, &init);
}

void board_uart_write(const char *text)
{
  if (text == NULL) {
    return;
  }

  while (*text != '\0') {
    EUSART_Tx(EUSART1, (uint8_t)*text);
    text++;
  }
}
