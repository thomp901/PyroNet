#ifndef SERVICES_RISK_PYRONET_RISK_ENGINE_H
#define SERVICES_RISK_PYRONET_RISK_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  PYRONET_RISK_LEVEL_1 = 1,
  PYRONET_RISK_LEVEL_2 = 2,
  PYRONET_RISK_LEVEL_3 = 3,
  PYRONET_RISK_LEVEL_4 = 4,
  PYRONET_RISK_LEVEL_5 = 5,
} pyronet_risk_level_t;

typedef enum {
  PYRONET_RISK_REASON_NONE = 0,
  PYRONET_RISK_REASON_THRESHOLD = 1,
  PYRONET_RISK_REASON_L4_VOC = 2,
  PYRONET_RISK_REASON_NEIGHBOR_ALERT = 3,
  PYRONET_RISK_REASON_L5_SENSOR = 4,
  PYRONET_RISK_REASON_DECAY = 5,
  PYRONET_RISK_REASON_OVERRIDE = 6,
  PYRONET_RISK_REASON_CONFIG_UPDATE = 7,
} pyronet_risk_reason_t;

typedef struct {
  uint32_t config_id;
  float l2_temp_c_min;
  float l2_rh_percent_max;
  float l2_voc_min;
  float l3_temp_c_min;
  float l3_rh_percent_max;
  float l3_voc_min;
  float l4_voc_min;
  float l5_voc_min;
  float l5_pm25_ug_m3_min;
  uint8_t normal_pm25_samples_per_day;
} pyronet_risk_config_t;

typedef struct {
  int64_t timestamp_ns;
  uint32_t config_id;
  pyronet_risk_level_t previous_level;
  pyronet_risk_level_t current_level;
  pyronet_risk_reason_t reason;
  bool override_active;
  bool has_air_quality;
  float temperature_c;
  float humidity_percent;
  float voc;
  bool has_pm25;
  float pm25_ug_m3;
} pyronet_risk_snapshot_t;

typedef struct {
  void *context;
  void (*send_report)(void *context,
                      const pyronet_risk_snapshot_t *snapshot,
                      bool level_changed);
  void (*send_sensor_alert)(void *context,
                            const pyronet_risk_snapshot_t *snapshot);
  void (*broadcast_neighbor_alert)(void *context,
                                   const pyronet_risk_snapshot_t *snapshot);
  void (*request_pm25_sample)(void *context);
  void (*set_pm25_schedule)(void *context, uint8_t samples_per_day);
  void (*set_report_interval)(void *context, int64_t interval_ns);
} pyronet_risk_engine_ops_t;

typedef struct {
  pyronet_risk_engine_ops_t ops;
  pyronet_risk_config_t config;
  pyronet_risk_level_t current_level;
  int64_t last_support_ns;
  int64_t neighbor_hold_until_ns;
  int64_t report_interval_ns;
  int64_t next_report_due_ns;
  bool has_air_quality;
  int64_t air_quality_timestamp_ns;
  float latest_temperature_c;
  float latest_humidity_percent;
  float latest_voc;
  bool has_pm25;
  int64_t pm25_timestamp_ns;
  float latest_pm25_ug_m3;
  bool override_active;
  pyronet_risk_level_t override_level;
} pyronet_risk_engine_t;

pyronet_risk_config_t pyronet_risk_config_default(void);
const char *pyronet_risk_reason_name(pyronet_risk_reason_t reason);

void pyronet_risk_engine_init(pyronet_risk_engine_t *engine,
                              const pyronet_risk_engine_ops_t *ops,
                              int64_t now_ns);
bool pyronet_risk_engine_apply_config_update(pyronet_risk_engine_t *engine,
                                             const pyronet_risk_config_t *config,
                                             int64_t now_ns);
void pyronet_risk_engine_submit_air_quality(pyronet_risk_engine_t *engine,
                                            int64_t timestamp_ns,
                                            float temperature_c,
                                            float humidity_percent,
                                            float voc);
void pyronet_risk_engine_submit_pm25(pyronet_risk_engine_t *engine,
                                     int64_t timestamp_ns,
                                     float pm25_ug_m3);
void pyronet_risk_engine_receive_neighbor_alert(pyronet_risk_engine_t *engine,
                                                int64_t now_ns);
void pyronet_risk_engine_set_override(pyronet_risk_engine_t *engine,
                                      bool active,
                                      pyronet_risk_level_t level,
                                      int64_t now_ns);
void pyronet_risk_engine_tick(pyronet_risk_engine_t *engine, int64_t now_ns);

pyronet_risk_level_t pyronet_risk_engine_current_level(
  const pyronet_risk_engine_t *engine);
const pyronet_risk_config_t *pyronet_risk_engine_config(
  const pyronet_risk_engine_t *engine);

#endif
