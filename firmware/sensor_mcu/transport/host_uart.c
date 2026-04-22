#include "transport/host_uart.h"

#include "em_cmu.h"
#include "em_eusart.h"
#include "em_gpio.h"

#define HOST_UART_PERIPHERAL EUSART1
#define HOST_UART_RX_PORT    gpioPortC
#define HOST_UART_RX_PIN     0U
#define HOST_UART_TX_PORT    gpioPortC
#define HOST_UART_TX_PIN     1U

static bool host_uart_initialized = false;

static void host_uart_configure_pins(void)
{
  GPIO_PinModeSet(HOST_UART_TX_PORT, HOST_UART_TX_PIN, gpioModePushPull, 1);
  GPIO_PinModeSet(HOST_UART_RX_PORT, HOST_UART_RX_PIN, gpioModeInputPull, 1);

  GPIO->EUSARTROUTE[EUSART_NUM(HOST_UART_PERIPHERAL)].TXROUTE =
    (HOST_UART_TX_PORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
    | (HOST_UART_TX_PIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(HOST_UART_PERIPHERAL)].RXROUTE =
    (HOST_UART_RX_PORT << _GPIO_EUSART_RXROUTE_PORT_SHIFT)
    | (HOST_UART_RX_PIN << _GPIO_EUSART_RXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[EUSART_NUM(HOST_UART_PERIPHERAL)].ROUTEEN =
    GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_RXPEN;
}

bool host_uart_init(void)
{
  EUSART_UartInit_TypeDef init = EUSART_UART_INIT_DEFAULT_HF;

  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_EUSART1, true);

  host_uart_configure_pins();

  init.enable = eusartDisable;
  init.baudrate = HOST_UART_BAUDRATE;
  EUSART_UartInitHf(HOST_UART_PERIPHERAL, &init);
  EUSART_Enable(HOST_UART_PERIPHERAL, eusartEnable);

  while ((HOST_UART_PERIPHERAL->STATUS & EUSART_STATUS_RXFL) != 0U) {
    (void)HOST_UART_PERIPHERAL->RXDATA;
  }

  host_uart_initialized = true;
  return true;
}

bool host_uart_try_read_byte(uint8_t *out_byte)
{
  if ((!host_uart_initialized) || (out_byte == NULL)) {
    return false;
  }

  if ((HOST_UART_PERIPHERAL->STATUS & EUSART_STATUS_RXFL) == 0U) {
    return false;
  }

  *out_byte = (uint8_t)HOST_UART_PERIPHERAL->RXDATA;
  return true;
}

void host_uart_write(const uint8_t *data, size_t length)
{
  size_t index;

  if ((!host_uart_initialized) || (data == NULL)) {
    return;
  }

  for (index = 0U; index < length; index++) {
    EUSART_Tx(HOST_UART_PERIPHERAL, data[index]);
  }
}
