#include "app/app_time_anchor.h"

#include <string.h>

void app_time_anchor_init(app_time_anchor_t *anchor)
{
  if (anchor == NULL) {
    return;
  }

  memset(anchor, 0, sizeof(*anchor));
}

void app_time_anchor_set(app_time_anchor_t *anchor,
                         int64_t now_ns,
                         uint32_t unix_time_s)
{
  if (anchor == NULL) {
    return;
  }

  anchor->valid = true;
  anchor->monotonic_anchor_ns = now_ns;
  anchor->unix_anchor_s = unix_time_s;
}

bool app_time_anchor_resolve(const app_time_anchor_t *anchor,
                             int64_t monotonic_timestamp_ns,
                             uint32_t *out_unix_time_s)
{
  int64_t delta_ns;
  uint64_t delta_s;

  if ((anchor == NULL)
      || (out_unix_time_s == NULL)
      || !anchor->valid) {
    return false;
  }

  delta_ns = monotonic_timestamp_ns - anchor->monotonic_anchor_ns;
  if (delta_ns >= 0) {
    *out_unix_time_s = anchor->unix_anchor_s
                       + (uint32_t)((uint64_t)delta_ns / 1000000000ULL);
    return true;
  }

  /*
   * TIME_SYNC_UPDATE can arrive after the underlying application event.
   * Preserve event-time ownership by walking backward from the anchor instead
   * of silently stamping retry time onto deferred packets.
   */
  delta_s = ((uint64_t)(-delta_ns)) / 1000000000ULL;
  if (delta_s > anchor->unix_anchor_s) {
    return false;
  }

  *out_unix_time_s = anchor->unix_anchor_s - (uint32_t)delta_s;
  return true;
}
