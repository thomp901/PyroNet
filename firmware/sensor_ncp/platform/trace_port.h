#ifndef TRACE_PORT_H
#define TRACE_PORT_H

#include <stdbool.h>

bool tracePortInit(void);
void tracePortWriteLine(const char *line);

#endif
