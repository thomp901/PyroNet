#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <ti/drivers/UART2.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)

#include <ti_drivers_config.h>
#include "diag/log.h"
#include "uart_transport.h"

#define UART_TX_DIO 28U
#define UART_RX_DIO 27U
#define UART_BAUD_RATE 115200U
#define UART_LINE_BUFFER_SIZE 64U
#define UART_RESPONSE_BUFFER_SIZE 128U
#define SENSOR_NCP_VERSION "sensor_ncp uart-bootstrap v0.1"

static void uartWrite(UART2_Handle uartHandle, const char *text)
{
    (void)UART2_write(uartHandle, text, strlen(text), NULL);
}

static void uartWriteLine(UART2_Handle uartHandle, const char *line)
{
    uartWrite(uartHandle, line);
    uartWrite(uartHandle, "\r\n");
}

static void processCommand(UART2_Handle uartHandle, const char *command)
{
    char lineBuffer[UART_RESPONSE_BUFFER_SIZE];

    diagLogf("UART_CMD", "%s", command);

    if (strcmp(command, "PING") == 0)
    {
        uartWriteLine(uartHandle, "PONG");
    }
    else if (strcmp(command, "GET_VERSION") == 0)
    {
        uartWriteLine(uartHandle, SENSOR_NCP_VERSION);
    }
    else if (strcmp(command, "GET_STATUS") == 0)
    {
        snprintf(lineBuffer,
                 sizeof(lineBuffer),
                 "STATUS ready uart=%lu tx_dio=%u rx_dio=%u",
                 (unsigned long)UART_BAUD_RATE,
                 UART_TX_DIO,
                 UART_RX_DIO);
        uartWriteLine(uartHandle, lineBuffer);
    }
    else if (strcmp(command, "RESET") == 0)
    {
        uartWriteLine(uartHandle, "RESETTING");
        usleep(20000);
        SysCtrlSystemReset();
    }
    else if (command[0] != '\0')
    {
        uartWriteLine(uartHandle, "ERR unknown_command");
    }
}

void uartTransportRun(void)
{
    UART2_Handle uartHandle;
    UART2_Params uartParams;
    char commandBuffer[UART_LINE_BUFFER_SIZE];
    size_t commandLength = 0;
    char lineBuffer[UART_RESPONSE_BUFFER_SIZE];
    uint8_t rxByte = 0;

    UART2_Params_init(&uartParams);
    uartParams.baudRate = UART_BAUD_RATE;
    uartParams.readReturnMode = UART2_ReadReturnMode_FULL;

    uartHandle = UART2_open(CONFIG_UART2_0, &uartParams);
    if (uartHandle == NULL)
    {
        diagLogInfo("UART_INIT", "failed");
        while (1) {}
    }

    snprintf(lineBuffer,
             sizeof(lineBuffer),
             "ready baud=%lu tx_dio=%u rx_dio=%u",
             (unsigned long)UART_BAUD_RATE,
             UART_TX_DIO,
             UART_RX_DIO);
    diagLogInfo("UART_INIT", lineBuffer);

    uartWriteLine(uartHandle, "READY sensor_ncp uart-bootstrap");
    uartWriteLine(uartHandle, "CMDS PING GET_VERSION GET_STATUS RESET");

    while (1)
    {
        if (UART2_read(uartHandle, &rxByte, sizeof(rxByte), NULL) != UART2_STATUS_SUCCESS)
        {
            uartWriteLine(uartHandle, "ERR uart_read");
            continue;
        }

        if (rxByte == '\r')
        {
            continue;
        }

        if (rxByte == '\n')
        {
            commandBuffer[commandLength] = '\0';
            processCommand(uartHandle, commandBuffer);
            commandLength = 0;
            continue;
        }

        if (commandLength >= (sizeof(commandBuffer) - 1U))
        {
            commandLength = 0;
            uartWriteLine(uartHandle, "ERR command_too_long");
            continue;
        }

        commandBuffer[commandLength++] = (char)rxByte;
    }
}
