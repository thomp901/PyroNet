#ifndef APP_APP_TIME_ANCHOR_H
#define APP_APP_TIME_ANCHOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool valid;
  int64_t monotonic_anchor_ns;
  uint32_t unix_anchor_s;
} app_time_anchor_t;

void app_time_anchor_init(app_time_anchor_t *anchor);
void app_time_anchor_set(app_time_anchor_t *anchor,
                         int64_t now_ns,
                         uint32_t unix_time_s);
bool app_time_anchor_resolve(const app_time_anchor_t *anchor,
                             int64_t monotonic_timestamp_ns,
                             uint32_t *out_unix_time_s);

#endif
