#ifndef APP_APP_BOUNDARY_TX_H
#define APP_APP_BOUNDARY_TX_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_provisioning.h"
#include "app/app_time_anchor.h"

typedef struct {
  bool valid;
  uint8_t reason;
} app_pending_registration_t;

typedef struct {
  bool valid;
  uint8_t change_reason;
  int64_t event_monotonic_ns;
} app_pending_parent_update_t;

typedef struct {
  bool registration_deferred_logged;
  bool parent_update_deferred_logged;
  app_pending_registration_t pending_registration;
  app_pending_parent_update_t pending_parent_update;
  uint32_t last_parent_update_timestamp_s;
  uint8_t last_parent_change_reason;
} app_boundary_tx_t;

void app_boundary_tx_init(app_boundary_tx_t *tx);
void app_boundary_tx_handle_registration_needed(
  app_boundary_tx_t *tx,
  const app_registration_identity_t *identity,
  uint8_t reason);
void app_boundary_tx_handle_parent_changed(
  app_boundary_tx_t *tx,
  const app_registration_identity_t *identity,
  const app_time_anchor_t *time_anchor,
  uint8_t change_reason,
  int64_t event_monotonic_ns);
void app_boundary_tx_flush(app_boundary_tx_t *tx,
                           const app_registration_identity_t *identity,
                           const app_time_anchor_t *time_anchor);

#endif
