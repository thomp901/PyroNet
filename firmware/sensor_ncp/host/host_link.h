#ifndef HOST_HOST_LINK_H
#define HOST_HOST_LINK_H

#include <stdbool.h>

#include "host_proto.h"

void host_link_init(void);
void host_link_poll(void);
bool host_link_is_ready(void);

#endif /* HOST_HOST_LINK_H */
