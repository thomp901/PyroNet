#ifndef TRANSPORT_HOST_LINK_H
#define TRANSPORT_HOST_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "transport/host_proto.h"

bool host_link_init(void);
void host_link_poll(void);
bool host_link_is_ready(void);
bool host_link_ping(uint32_t token);
bool host_link_get_status(host_status_v1_t *out_status);

#endif
