#include <stdarg.h>
#include <stdio.h>

#include "log.h"
#include "../platform/trace_port.h"

#define DIAG_LOG_MESSAGE_BUFFER_SIZE 96U
#define DIAG_LOG_LINE_BUFFER_SIZE 128U

static void writeTaggedLine(const char *tag, const char *message)
{
    char lineBuffer[DIAG_LOG_LINE_BUFFER_SIZE];

    if ((tag == NULL) || (tag[0] == '\0'))
    {
        diagLogLine(message);
        return;
    }

    snprintf(lineBuffer, sizeof(lineBuffer), "%s: %s", tag, message);
    diagLogLine(lineBuffer);
}

bool diagLogInit(void)
{
    return tracePortInit();
}

void diagLogLine(const char *line)
{
    tracePortWriteLine(line);
}

void diagLogInfo(const char *tag, const char *message)
{
    writeTaggedLine(tag, message);
}

void diagLogf(const char *tag, const char *format, ...)
{
    char messageBuffer[DIAG_LOG_MESSAGE_BUFFER_SIZE];
    va_list args;

    va_start(args, format);
    vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
    va_end(args);

    writeTaggedLine(tag, messageBuffer);
}
