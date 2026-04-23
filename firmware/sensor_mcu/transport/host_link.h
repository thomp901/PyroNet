#ifndef TRANSPORT_HOST_LINK_H
#define TRANSPORT_HOST_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "transport/host_proto.h"

typedef struct {
  void *context;
  void (*on_registration_needed)(
    void *context,
    const pyronet_host_registration_needed_v1_t *event);
  void (*on_parent_changed)(
    void *context,
    const pyronet_host_parent_changed_v1_t *event);
  void (*on_tx_result)(void *context, const pyronet_host_tx_result_v1_t *event);
  void (*on_time_sync_update)(
    void *context,
    const pyronet_host_time_sync_update_v1_t *event);
  void (*on_neighbor_alert_rx)(
    void *context,
    const pyronet_host_neighbor_alert_received_v1_t *event);
  void (*on_config_update_rx)(
    void *context,
    const pyronet_host_config_update_received_v1_t *event);
} host_link_event_handlers_t;

bool host_link_init(const host_link_event_handlers_t *event_handlers);
void host_link_poll(void);
bool host_link_is_ready(void);
bool host_link_ping(uint32_t token);
bool host_link_get_status(host_status_v1_t *out_status);
bool host_link_send_registration(
  const pyronet_host_send_registration_v1_t *payload);
bool host_link_send_sensor_report(
  const pyronet_host_send_sensor_report_v1_t *payload);
bool host_link_send_sensor_alert(
  const pyronet_host_send_sensor_alert_v1_t *payload);
bool host_link_send_neighbor_alert(
  const pyronet_host_send_neighbor_alert_v1_t *payload);
bool host_link_request_parent_update(
  const pyronet_host_request_parent_update_v1_t *payload);

#endif
