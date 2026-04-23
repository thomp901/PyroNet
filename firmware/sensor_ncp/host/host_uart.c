#include "host_uart.h"

#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26X2.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include DeviceFamily_constructPath(driverlib/uart.h)

#include <ti_drivers_config.h>

static bool hostUartInitialized = false;

bool host_uart_init(void)
{
    if (hostUartInitialized)
    {
        return true;
    }

    (void)Power_setDependency(PowerCC26XX_PERIPH_UART0);
    (void)Power_setConstraint(PowerCC26XX_DISALLOW_STANDBY);

    IOCPortConfigureSet(CONFIG_PIN_UART_TX, IOC_PORT_MCU_UART0_TX, IOC_STD_OUTPUT);
    IOCPortConfigureSet(CONFIG_PIN_UART_RX, IOC_PORT_MCU_UART0_RX, IOC_STD_INPUT);

    UARTDisable(UART0_BASE);
    UARTConfigSetExpClk(UART0_BASE,
                        SysCtrlClockGet(),
                        HOST_UART_BAUD_RATE,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    UARTEnable(UART0_BASE);

    while (UARTCharsAvail(UART0_BASE))
    {
        (void)UARTCharGetNonBlocking(UART0_BASE);
    }

    hostUartInitialized = true;
    return true;
}

bool host_uart_read_byte(uint8_t *out_byte)
{
    int32_t value;

    if ((!hostUartInitialized) || (out_byte == NULL))
    {
        return false;
    }

    if (!UARTCharsAvail(UART0_BASE))
    {
        return false;
    }

    value = UARTCharGetNonBlocking(UART0_BASE);
    if (value < 0)
    {
        return false;
    }

    *out_byte = (uint8_t)value;
    return true;
}

bool host_uart_write(const uint8_t *data, size_t length)
{
    size_t index;

    if ((!hostUartInitialized) || ((data == NULL) && (length > 0U)))
    {
        return false;
    }

    for (index = 0; index < length; ++index)
    {
        UARTCharPut(UART0_BASE, data[index]);
    }

    return true;
}
