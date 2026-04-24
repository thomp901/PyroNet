#ifndef PYRONET_DEVICE_H
#define PYRONET_DEVICE_H

#include <stdbool.h>
#include "sl_wisun_api.h"

void pyronet_device_init(void);
void pyronet_device_process(void);
void pyronet_device_on_join_requested(void);
void pyronet_device_on_connected(void);
void pyronet_device_on_disconnected(void);
bool pyronet_device_handle_socket_data(sl_wisun_evt_t *evt);
bool pyronet_device_handle_socket_data_sent(sl_wisun_evt_t *evt);

#endif
