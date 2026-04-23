#include "app/app_boundary_tx.h"

#include <stdio.h>
#include <string.h>

#include "transport/host_link.h"

static bool app_boundary_tx_try_send_registration(
  app_boundary_tx_t *tx,
  const app_registration_identity_t *identity,
  uint8_t reason)
{
  pyronet_host_send_registration_v1_t payload;

  if (tx == NULL) {
    return false;
  }

  if (!app_provisioning_identity_valid(identity)) {
    tx->pending_registration.valid = true;
    tx->pending_registration.reason = reason;
    if (!tx->registration_deferred_logged) {
      printf("SEND_REGISTRATION_DEFERRED reason=%u missing=identity\r\n", reason);
      tx->registration_deferred_logged = true;
    }
    return false;
  }

  payload.node_id = identity->node_id;
  payload.latitude = identity->latitude;
  payload.longitude = identity->longitude;
  payload.fw_version = identity->fw_version_8_8;
  payload.battery_pct = identity->battery_pct;

  if (!host_link_send_registration(&payload)) {
    printf("SEND_REGISTRATION_FAILED reason=%u\r\n", reason);
    return false;
  }

  tx->pending_registration.valid = false;
  tx->registration_deferred_logged = false;
  return true;
}

static bool app_boundary_tx_try_send_parent_update(
  app_boundary_tx_t *tx,
  const app_registration_identity_t *identity,
  const app_time_anchor_t *time_anchor,
  uint8_t change_reason,
  int64_t event_monotonic_ns)
{
  pyronet_host_request_parent_update_v1_t payload;
  uint32_t timestamp_s;

  if (tx == NULL) {
    return false;
  }

  if (!app_provisioning_identity_valid(identity)) {
    tx->pending_parent_update.valid = true;
    tx->pending_parent_update.change_reason = change_reason;
    tx->pending_parent_update.event_monotonic_ns = event_monotonic_ns;
    if (!tx->parent_update_deferred_logged) {
      printf("REQUEST_PARENT_UPDATE_DEFERRED reason=%u missing=identity\r\n",
             change_reason);
      tx->parent_update_deferred_logged = true;
    }
    return false;
  }

  if (!app_time_anchor_resolve(time_anchor, event_monotonic_ns, &timestamp_s)) {
    tx->pending_parent_update.valid = true;
    tx->pending_parent_update.change_reason = change_reason;
    tx->pending_parent_update.event_monotonic_ns = event_monotonic_ns;
    if (!tx->parent_update_deferred_logged) {
      printf("REQUEST_PARENT_UPDATE_DEFERRED reason=%u missing=unix-time\r\n",
             change_reason);
      tx->parent_update_deferred_logged = true;
    }
    return false;
  }

  payload.node_id = identity->node_id;
  payload.timestamp = timestamp_s;

  tx->last_parent_change_reason = change_reason;
  tx->last_parent_update_timestamp_s = payload.timestamp;

  if (!host_link_request_parent_update(&payload)) {
    printf("REQUEST_PARENT_UPDATE_FAILED reason=%u\r\n", change_reason);
    return false;
  }

  tx->pending_parent_update.valid = false;
  tx->parent_update_deferred_logged = false;
  return true;
}

void app_boundary_tx_init(app_boundary_tx_t *tx)
{
  if (tx == NULL) {
    return;
  }

  memset(tx, 0, sizeof(*tx));
}

void app_boundary_tx_handle_registration_needed(
  app_boundary_tx_t *tx,
  const app_registration_identity_t *identity,
  uint8_t reason)
{
  (void)app_boundary_tx_try_send_registration(tx, identity, reason);
}

void app_boundary_tx_handle_parent_changed(
  app_boundary_tx_t *tx,
  const app_registration_identity_t *identity,
  const app_time_anchor_t *time_anchor,
  uint8_t change_reason,
  int64_t event_monotonic_ns)
{
  if (tx == NULL) {
    return;
  }

  tx->last_parent_change_reason = change_reason;
  tx->pending_parent_update.valid = true;
  tx->pending_parent_update.change_reason = change_reason;
  tx->pending_parent_update.event_monotonic_ns = event_monotonic_ns;

  (void)app_boundary_tx_try_send_parent_update(tx,
                                               identity,
                                               time_anchor,
                                               change_reason,
                                               event_monotonic_ns);
}

void app_boundary_tx_flush(app_boundary_tx_t *tx,
                           const app_registration_identity_t *identity,
                           const app_time_anchor_t *time_anchor)
{
  if (tx == NULL) {
    return;
  }

  if (tx->pending_registration.valid) {
    (void)app_boundary_tx_try_send_registration(tx,
                                                identity,
                                                tx->pending_registration.reason);
  }

  if (tx->pending_parent_update.valid) {
    (void)app_boundary_tx_try_send_parent_update(
      tx,
      identity,
      time_anchor,
      tx->pending_parent_update.change_reason,
      tx->pending_parent_update.event_monotonic_ns);
  }
}
