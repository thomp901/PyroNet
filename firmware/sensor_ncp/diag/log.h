#ifndef DIAG_LOG_H
#define DIAG_LOG_H

#include <stdbool.h>

bool diagLogInit(void);
void diagLogLine(const char *line);
void diagLogInfo(const char *tag, const char *message);
void diagLogf(const char *tag, const char *format, ...);

#endif
