#include "services/risk/pyronet_risk_engine.h"

#include <stddef.h>
#include <string.h>

#define PYRONET_RISK_MINUTE_NS          60000000000LL
#define PYRONET_RISK_HOUR_NS            (60LL * PYRONET_RISK_MINUTE_NS)
#define PYRONET_RISK_NEIGHBOR_HOLD_NS   (2LL * PYRONET_RISK_MINUTE_NS)

static int64_t pyronet_risk_report_interval_for_level(pyronet_risk_level_t level)
{
  (void)level;

  return 8LL * PYRONET_RISK_HOUR_NS;
}

static pyronet_risk_reason_t pyronet_risk_reason_for_level(
  pyronet_risk_level_t level,
  pyronet_risk_reason_t fallback_reason)
{
  switch (level) {
    case PYRONET_RISK_LEVEL_4:
      return PYRONET_RISK_REASON_L4_VOC;
    case PYRONET_RISK_LEVEL_5:
      return PYRONET_RISK_REASON_L5_SENSOR;
    case PYRONET_RISK_LEVEL_1:
    case PYRONET_RISK_LEVEL_2:
    case PYRONET_RISK_LEVEL_3:
    default:
      return fallback_reason;
  }
}

static void pyronet_risk_snapshot_fill(const pyronet_risk_engine_t *engine,
                                       pyronet_risk_snapshot_t *snapshot,
                                       int64_t timestamp_ns,
                                       pyronet_risk_level_t previous_level,
                                       pyronet_risk_reason_t reason)
{
  snapshot->timestamp_ns = timestamp_ns;
  snapshot->config_id = engine->config.config_id;
  snapshot->previous_level = previous_level;
  snapshot->current_level = engine->current_level;
  snapshot->reason = reason;
  snapshot->override_active = engine->override_active;
  snapshot->has_air_quality = engine->has_air_quality;
  snapshot->temperature_c = engine->latest_temperature_c;
  snapshot->humidity_percent = engine->latest_humidity_percent;
  snapshot->voc = engine->latest_voc;
  snapshot->has_pm25 = engine->has_pm25;
  snapshot->pm25_ug_m3 = engine->latest_pm25_ug_m3;
}

static pyronet_risk_level_t pyronet_risk_live_sensor_level(
  const pyronet_risk_engine_t *engine)
{
  pyronet_risk_level_t level = PYRONET_RISK_LEVEL_1;

  if (!engine->has_air_quality) {
    return level;
  }

  if ((engine->latest_temperature_c >= engine->config.l2_temp_c_min)
      && (engine->latest_humidity_percent <= engine->config.l2_rh_percent_max)
      && (engine->latest_voc >= engine->config.l2_voc_min)) {
    level = PYRONET_RISK_LEVEL_2;
  }

  if ((engine->latest_temperature_c >= engine->config.l3_temp_c_min)
      && (engine->latest_humidity_percent <= engine->config.l3_rh_percent_max)
      && (engine->latest_voc >= engine->config.l3_voc_min)) {
    level = PYRONET_RISK_LEVEL_3;
  }

  if (engine->latest_voc >= engine->config.l4_voc_min) {
    level = PYRONET_RISK_LEVEL_4;
  }

  if ((engine->latest_voc >= engine->config.l5_voc_min)
      && engine->has_pm25
      && (engine->latest_pm25_ug_m3 >= engine->config.l5_pm25_ug_m3_min)) {
    level = PYRONET_RISK_LEVEL_5;
  }

  return level;
}

static pyronet_risk_level_t pyronet_risk_supported_level_now(
  const pyronet_risk_engine_t *engine,
  int64_t now_ns)
{
  pyronet_risk_level_t level;

  if (engine->override_active) {
    return engine->override_level;
  }

  if (now_ns < engine->neighbor_hold_until_ns) {
    return PYRONET_RISK_LEVEL_4;
  }

  level = pyronet_risk_live_sensor_level(engine);

  return level;
}

static void pyronet_risk_apply_policy(pyronet_risk_engine_t *engine,
                                      pyronet_risk_level_t previous_level,
                                      int64_t now_ns)
{
  int64_t interval_ns = pyronet_risk_report_interval_for_level(
    engine->current_level);

  (void)previous_level;

  engine->report_interval_ns = interval_ns;
  engine->next_report_due_ns = now_ns + interval_ns;
}

static void pyronet_risk_send_report(pyronet_risk_engine_t *engine,
                                     int64_t now_ns,
                                     pyronet_risk_level_t previous_level,
                                     pyronet_risk_reason_t reason,
                                     bool level_changed)
{
  pyronet_risk_snapshot_t snapshot = { 0 };

  if (engine->ops.send_report == NULL) {
    return;
  }

  pyronet_risk_snapshot_fill(engine, &snapshot, now_ns, previous_level, reason);
  engine->ops.send_report(engine->ops.context, &snapshot, level_changed);
}

static void pyronet_risk_send_sensor_alert(pyronet_risk_engine_t *engine,
                                           int64_t now_ns,
                                           pyronet_risk_level_t previous_level,
                                           pyronet_risk_reason_t reason)
{
  pyronet_risk_snapshot_t snapshot = { 0 };

  if (engine->ops.send_sensor_alert == NULL) {
    return;
  }

  pyronet_risk_snapshot_fill(engine, &snapshot, now_ns, previous_level, reason);
  engine->ops.send_sensor_alert(engine->ops.context, &snapshot);
}

static void pyronet_risk_broadcast_neighbor_alert(pyronet_risk_engine_t *engine,
                                                  int64_t now_ns,
                                                  pyronet_risk_level_t previous_level,
                                                  pyronet_risk_reason_t reason)
{
  pyronet_risk_snapshot_t snapshot = { 0 };

  if (engine->ops.broadcast_neighbor_alert == NULL) {
    return;
  }

  pyronet_risk_snapshot_fill(engine, &snapshot, now_ns, previous_level, reason);
  engine->ops.broadcast_neighbor_alert(engine->ops.context, &snapshot);
}

static void pyronet_risk_transition(pyronet_risk_engine_t *engine,
                                    pyronet_risk_level_t next_level,
                                    int64_t now_ns,
                                    pyronet_risk_reason_t reason,
                                    bool trigger_critical_alerts)
{
  pyronet_risk_level_t previous_level = engine->current_level;

  if (next_level == previous_level) {
    return;
  }

  engine->current_level = next_level;

  engine->last_support_ns = now_ns;

  pyronet_risk_apply_policy(engine, previous_level, now_ns);

  if (trigger_critical_alerts && (next_level == PYRONET_RISK_LEVEL_5)) {
    pyronet_risk_send_sensor_alert(engine, now_ns, previous_level, reason);
    pyronet_risk_broadcast_neighbor_alert(engine, now_ns, previous_level, reason);
  }

  pyronet_risk_send_report(engine, now_ns, previous_level, reason, true);
}

static void pyronet_risk_reconcile(pyronet_risk_engine_t *engine,
                                   int64_t now_ns,
                                   pyronet_risk_level_t event_support_level,
                                   pyronet_risk_reason_t promotion_reason,
                                   bool allow_critical_alerts)
{
  pyronet_risk_level_t supported_level = pyronet_risk_supported_level_now(engine,
                                                                          now_ns);

  (void)event_support_level;

  if (engine->override_active) {
    pyronet_risk_transition(engine,
                            engine->override_level,
                            now_ns,
                            PYRONET_RISK_REASON_OVERRIDE,
                            false);
    return;
  }

  if (supported_level != engine->current_level) {
    pyronet_risk_transition(engine,
                            supported_level,
                            now_ns,
                            pyronet_risk_reason_for_level(supported_level,
                                                          promotion_reason),
                            allow_critical_alerts && (supported_level == PYRONET_RISK_LEVEL_5));
  }
}

pyronet_risk_config_t pyronet_risk_config_default(void)
{
  pyronet_risk_config_t config = {
    .config_id = 0U,
    .l2_temp_c_min = 35.0f,
    .l2_rh_percent_max = 40.0f,
    .l2_voc_min = 100.0f,
    .l3_temp_c_min = 45.0f,
    .l3_rh_percent_max = 25.0f,
    .l3_voc_min = 200.0f,
    .l4_voc_min = 300.0f,
    .l5_voc_min = 500.0f,
    .l5_pm25_ug_m3_min = 35.0f,
    .normal_pm25_samples_per_day = 0U,
  };

  return config;
}

const char *pyronet_risk_reason_name(pyronet_risk_reason_t reason)
{
  switch (reason) {
    case PYRONET_RISK_REASON_THRESHOLD:
      return "threshold";
    case PYRONET_RISK_REASON_L4_VOC:
      return "l4-voc";
    case PYRONET_RISK_REASON_NEIGHBOR_ALERT:
      return "neighbor-alert";
    case PYRONET_RISK_REASON_L5_SENSOR:
      return "l5-sensor";
    case PYRONET_RISK_REASON_DECAY:
      return "decay";
    case PYRONET_RISK_REASON_OVERRIDE:
      return "override";
    case PYRONET_RISK_REASON_CONFIG_UPDATE:
      return "config-update";
    case PYRONET_RISK_REASON_NONE:
    default:
      return "none";
  }
}

void pyronet_risk_engine_init(pyronet_risk_engine_t *engine,
                              const pyronet_risk_engine_ops_t *ops,
                              int64_t now_ns)
{
  if (engine == NULL) {
    return;
  }

  memset(engine, 0, sizeof(*engine));

  if (ops != NULL) {
    engine->ops = *ops;
  }

  engine->config = pyronet_risk_config_default();
  engine->current_level = PYRONET_RISK_LEVEL_1;
  engine->last_support_ns = now_ns;
  engine->report_interval_ns = pyronet_risk_report_interval_for_level(engine->current_level);
  engine->next_report_due_ns = now_ns + engine->report_interval_ns;
}

bool pyronet_risk_engine_apply_config_update(pyronet_risk_engine_t *engine,
                                             const pyronet_risk_config_t *config,
                                             int64_t now_ns)
{
  pyronet_risk_level_t supported_level;

  if ((engine == NULL) || (config == NULL)) {
    return false;
  }

  if (config->config_id <= engine->config.config_id) {
    return false;
  }

  engine->config = *config;

  if (engine->override_active) {
    pyronet_risk_transition(engine,
                            engine->override_level,
                            now_ns,
                            PYRONET_RISK_REASON_OVERRIDE,
                            false);
    return true;
  }

  supported_level = pyronet_risk_supported_level_now(engine, now_ns);
  pyronet_risk_transition(engine,
                          supported_level,
                          now_ns,
                          PYRONET_RISK_REASON_CONFIG_UPDATE,
                          false);
  return true;
}

void pyronet_risk_engine_submit_air_quality(pyronet_risk_engine_t *engine,
                                            int64_t timestamp_ns,
                                            float temperature_c,
                                            float humidity_percent,
                                            float voc)
{
  pyronet_risk_level_t event_support_level;
  pyronet_risk_reason_t reason = PYRONET_RISK_REASON_THRESHOLD;

  if (engine == NULL) {
    return;
  }

  engine->has_air_quality = true;
  engine->air_quality_timestamp_ns = timestamp_ns;
  engine->latest_temperature_c = temperature_c;
  engine->latest_humidity_percent = humidity_percent;
  engine->latest_voc = voc;

  event_support_level = pyronet_risk_live_sensor_level(engine);
  if (event_support_level >= PYRONET_RISK_LEVEL_5) {
    reason = PYRONET_RISK_REASON_L5_SENSOR;
  } else if (event_support_level >= PYRONET_RISK_LEVEL_4) {
    reason = PYRONET_RISK_REASON_L4_VOC;
  }

  pyronet_risk_reconcile(engine,
                         timestamp_ns,
                         event_support_level,
                         reason,
                         true);
}

void pyronet_risk_engine_submit_pm25(pyronet_risk_engine_t *engine,
                                     int64_t timestamp_ns,
                                     float pm25_ug_m3)
{
  pyronet_risk_level_t event_support_level = 0;

  if (engine == NULL) {
    return;
  }

  engine->has_pm25 = true;
  engine->pm25_timestamp_ns = timestamp_ns;
  engine->latest_pm25_ug_m3 = pm25_ug_m3;

  if (!engine->override_active
      && engine->has_air_quality
      && (engine->latest_voc >= engine->config.l5_voc_min)
      && (engine->latest_pm25_ug_m3 >= engine->config.l5_pm25_ug_m3_min)) {
    event_support_level = PYRONET_RISK_LEVEL_5;
  }

  pyronet_risk_reconcile(engine,
                         timestamp_ns,
                         event_support_level,
                         PYRONET_RISK_REASON_L5_SENSOR,
                         true);
}

void pyronet_risk_engine_receive_neighbor_alert(pyronet_risk_engine_t *engine,
                                                int64_t now_ns)
{
  if (engine == NULL) {
    return;
  }

  engine->neighbor_hold_until_ns = now_ns + PYRONET_RISK_NEIGHBOR_HOLD_NS;
  if (engine->override_active) {
    pyronet_risk_reconcile(engine,
                           now_ns,
                           PYRONET_RISK_LEVEL_4,
                           PYRONET_RISK_REASON_NEIGHBOR_ALERT,
                           false);
    return;
  }

  pyronet_risk_transition(engine,
                          PYRONET_RISK_LEVEL_4,
                          now_ns,
                          PYRONET_RISK_REASON_NEIGHBOR_ALERT,
                          false);
}

void pyronet_risk_engine_set_override(pyronet_risk_engine_t *engine,
                                      bool active,
                                      pyronet_risk_level_t level,
                                      int64_t now_ns)
{
  pyronet_risk_level_t supported_level;

  if (engine == NULL) {
    return;
  }

  engine->override_active = active;
  engine->override_level = level;

  if (active) {
    pyronet_risk_transition(engine,
                            level,
                            now_ns,
                            PYRONET_RISK_REASON_OVERRIDE,
                            false);
    return;
  }

  supported_level = pyronet_risk_supported_level_now(engine, now_ns);
  pyronet_risk_transition(engine,
                          supported_level,
                          now_ns,
                          PYRONET_RISK_REASON_OVERRIDE,
                          false);
}

void pyronet_risk_engine_tick(pyronet_risk_engine_t *engine, int64_t now_ns)
{
  if (engine == NULL) {
    return;
  }

  pyronet_risk_reconcile(engine,
                         now_ns,
                         0,
                         PYRONET_RISK_REASON_NONE,
                         false);

  if ((engine->ops.send_report != NULL) && (now_ns >= engine->next_report_due_ns)) {
    pyronet_risk_send_report(engine,
                             now_ns,
                             engine->current_level,
                             PYRONET_RISK_REASON_NONE,
                             false);
    engine->next_report_due_ns = now_ns + engine->report_interval_ns;
  }
}

pyronet_risk_level_t pyronet_risk_engine_current_level(
  const pyronet_risk_engine_t *engine)
{
  return (engine != NULL) ? engine->current_level : PYRONET_RISK_LEVEL_1;
}

const pyronet_risk_config_t *pyronet_risk_engine_config(
  const pyronet_risk_engine_t *engine)
{
  return (engine != NULL) ? &engine->config : NULL;
}
