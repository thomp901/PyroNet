#include <FreeRTOS.h>
#include <task.h>

#include "swo_debug.h"

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    (void)swoDebugPrintf("PYRONET_FREERTOS_STACK_OVERFLOW task=%s handle=%p",
                         (pcTaskName != NULL) ? pcTaskName : "-",
                         (void *)pxTask);

    taskDISABLE_INTERRUPTS();
    while (1) {}
}
