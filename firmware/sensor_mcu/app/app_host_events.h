#ifndef APP_APP_HOST_EVENTS_H
#define APP_APP_HOST_EVENTS_H

#include "app/app_state.h"
#include "transport/host_link.h"

void app_host_events_init_handlers(host_link_event_handlers_t *handlers,
                                   app_context_t *context);

#endif
