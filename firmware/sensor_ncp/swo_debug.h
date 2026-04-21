#ifndef SWO_DEBUG_H
#define SWO_DEBUG_H

#include <stdbool.h>
#include <stddef.h>

bool swoDebugInit(void);
void swoDebugInitOrDie(void);
bool swoDebugWrite(const char *data, size_t length);
bool swoDebugWriteLine(const char *line);
bool swoDebugPrintf(const char *fmt, ...);
void swoDebugBootSelfTest(const char *phase, const char *detail);

#endif /* SWO_DEBUG_H */
