#include "services/risk/pyronet_risk_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MINUTE_NS  60000000000LL
#define TEST_HOUR_NS    (60LL * TEST_MINUTE_NS)

typedef struct {
  int report_count;
  int periodic_report_count;
  int level_change_report_count;
  int sensor_alert_count;
  int neighbor_alert_count;
  int pm25_request_count;
  int pm25_schedule_count;
  int report_interval_count;
  uint8_t last_pm25_schedule;
  int64_t last_report_interval_ns;
  pyronet_risk_snapshot_t last_report;
} test_callbacks_t;

static void test_fail(const char *message)
{
  fprintf(stderr, "FAIL: %s\n", message);
  exit(1);
}

static void test_expect_true(int condition, const char *message)
{
  if (!condition) {
    test_fail(message);
  }
}

static void test_expect_level(const pyronet_risk_engine_t *engine,
                              pyronet_risk_level_t level,
                              const char *message)
{
  if (pyronet_risk_engine_current_level(engine) != level) {
    test_fail(message);
  }
}

static void test_send_report(void *context,
                             const pyronet_risk_snapshot_t *snapshot,
                             bool level_changed)
{
  test_callbacks_t *callbacks = context;

  callbacks->report_count++;
  callbacks->level_change_report_count += level_changed ? 1 : 0;
  callbacks->periodic_report_count += level_changed ? 0 : 1;
  callbacks->last_report = *snapshot;
}

static void test_send_sensor_alert(void *context,
                                   const pyronet_risk_snapshot_t *snapshot)
{
  test_callbacks_t *callbacks = context;

  callbacks->sensor_alert_count++;
  callbacks->last_report = *snapshot;
}

static void test_broadcast_neighbor_alert(void *context,
                                          const pyronet_risk_snapshot_t *snapshot)
{
  test_callbacks_t *callbacks = context;

  callbacks->neighbor_alert_count++;
  callbacks->last_report = *snapshot;
}

static void test_request_pm25_sample(void *context)
{
  test_callbacks_t *callbacks = context;

  callbacks->pm25_request_count++;
}

static void test_set_pm25_schedule(void *context, uint8_t samples_per_day)
{
  test_callbacks_t *callbacks = context;

  callbacks->pm25_schedule_count++;
  callbacks->last_pm25_schedule = samples_per_day;
}

static void test_set_report_interval(void *context, int64_t interval_ns)
{
  test_callbacks_t *callbacks = context;

  callbacks->report_interval_count++;
  callbacks->last_report_interval_ns = interval_ns;
}

static pyronet_risk_engine_t test_engine_init(test_callbacks_t *callbacks)
{
  pyronet_risk_engine_t engine;
  pyronet_risk_engine_ops_t ops = {
    .context = callbacks,
    .send_report = test_send_report,
    .send_sensor_alert = test_send_sensor_alert,
    .broadcast_neighbor_alert = test_broadcast_neighbor_alert,
    .request_pm25_sample = test_request_pm25_sample,
    .set_pm25_schedule = test_set_pm25_schedule,
    .set_report_interval = test_set_report_interval,
  };

  memset(callbacks, 0, sizeof(*callbacks));
  pyronet_risk_engine_init(&engine, &ops, 0);
  return engine;
}

static void test_stale_config_is_rejected(void)
{
  test_callbacks_t callbacks;
  pyronet_risk_engine_t engine = test_engine_init(&callbacks);
  pyronet_risk_config_t config = pyronet_risk_config_default();

  config.config_id = 2U;
  config.l4_voc_min = 250.0f;
  test_expect_true(pyronet_risk_engine_apply_config_update(&engine, &config, 1),
                   "new config should be accepted");
  test_expect_true(!pyronet_risk_engine_apply_config_update(&engine, &config, 2),
                   "duplicate config should be rejected");

  config.config_id = 1U;
  config.l4_voc_min = 200.0f;
  test_expect_true(!pyronet_risk_engine_apply_config_update(&engine, &config, 3),
                   "older config should be rejected");
  test_expect_true(pyronet_risk_engine_config(&engine)->l4_voc_min == 250.0f,
                   "accepted config must remain active");
}

static void test_threshold_and_l4_progression(void)
{
  test_callbacks_t callbacks;
  pyronet_risk_engine_t engine = test_engine_init(&callbacks);

  pyronet_risk_engine_submit_air_quality(&engine, 5 * TEST_MINUTE_NS, 46.0f, 20.0f, 220.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_3,
                    "L3 thresholds should promote to level 3");
  test_expect_true(callbacks.level_change_report_count == 1,
                   "level change should send an immediate report");

  pyronet_risk_engine_submit_air_quality(&engine, 10 * TEST_MINUTE_NS, 48.0f, 20.0f, 320.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_4,
                    "VOC should promote to level 4");
  test_expect_true(callbacks.pm25_request_count == 0,
                   "risk engine should not drive immediate PM2.5 sampling");
  test_expect_true(callbacks.pm25_schedule_count == 0,
                   "risk engine should not drive PM2.5 sampling cadence");
  test_expect_true(callbacks.report_interval_count == 0,
                   "risk engine should not drive report interval callbacks");
}

static void test_l5_sensor_entry_sends_alerts(void)
{
  test_callbacks_t callbacks;
  pyronet_risk_engine_t engine = test_engine_init(&callbacks);

  pyronet_risk_engine_submit_air_quality(&engine, 5 * TEST_MINUTE_NS, 40.0f, 30.0f, 520.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_4,
                    "high VOC alone should reach level 4");

  pyronet_risk_engine_submit_pm25(&engine, 5 * TEST_MINUTE_NS + 1, 40.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_5,
                    "VOC and PM2.5 together should reach level 5");
  test_expect_true(callbacks.sensor_alert_count == 1,
                   "sensor-driven L5 entry must send a sensor alert");
  test_expect_true(callbacks.neighbor_alert_count == 1,
                   "sensor-driven L5 entry must broadcast a neighbor alert");
  test_expect_true(callbacks.pm25_schedule_count == 0,
                   "L5 entry should not change PM schedule");
}

static void test_manual_override_does_not_alert(void)
{
  test_callbacks_t callbacks;
  pyronet_risk_engine_t engine = test_engine_init(&callbacks);

  pyronet_risk_engine_set_override(&engine, true, PYRONET_RISK_LEVEL_5, 1);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_5,
                    "override should force level 5");
  test_expect_true(callbacks.sensor_alert_count == 0,
                   "manual override must not send a critical sensor alert");
  test_expect_true(callbacks.neighbor_alert_count == 0,
                   "manual override must not broadcast a neighbor alert");
}

static void test_local_readings_demote_immediately(void)
{
  test_callbacks_t callbacks;
  pyronet_risk_engine_t engine = test_engine_init(&callbacks);
  int64_t entry_ns = 5 * TEST_MINUTE_NS;

  pyronet_risk_engine_submit_air_quality(&engine, entry_ns, 40.0f, 30.0f, 520.0f);
  pyronet_risk_engine_submit_pm25(&engine, entry_ns + 1, 40.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_5,
                    "setup should reach level 5");

  pyronet_risk_engine_submit_pm25(&engine, entry_ns + TEST_MINUTE_NS, 10.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_4,
                    "L5 should demote immediately when PM2.5 no longer supports L5");

  pyronet_risk_engine_submit_air_quality(&engine,
                                         entry_ns + (2 * TEST_MINUTE_NS),
                                         46.0f,
                                         20.0f,
                                         220.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_3,
                    "risk should track current local thresholds immediately");

  pyronet_risk_engine_submit_air_quality(&engine,
                                         entry_ns + (3 * TEST_MINUTE_NS),
                                         24.0f,
                                         60.0f,
                                         40.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_1,
                    "normal local readings should return to L1 immediately");
}

static void test_neighbor_alert_locks_risk_to_l4_for_two_minutes(void)
{
  test_callbacks_t callbacks;
  pyronet_risk_engine_t engine = test_engine_init(&callbacks);
  int64_t alert_ns = 5 * TEST_MINUTE_NS;

  pyronet_risk_engine_receive_neighbor_alert(&engine, alert_ns);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_4,
                    "neighbor alert should lock risk to level 4");
  test_expect_true(callbacks.level_change_report_count == 1,
                   "neighbor-alert lock should send an immediate level-change report");
  test_expect_true(callbacks.last_report.reason == PYRONET_RISK_REASON_NEIGHBOR_ALERT,
                   "neighbor-alert lock should report the neighbor-alert reason");

  pyronet_risk_engine_submit_air_quality(&engine,
                                         alert_ns + TEST_MINUTE_NS,
                                         24.0f,
                                         60.0f,
                                         40.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_4,
                    "normal local readings should not clear the L4 neighbor-alert lock");

  pyronet_risk_engine_submit_air_quality(&engine,
                                         alert_ns + TEST_MINUTE_NS + 1,
                                         40.0f,
                                         30.0f,
                                         520.0f);
  pyronet_risk_engine_submit_pm25(&engine, alert_ns + TEST_MINUTE_NS + 2, 40.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_4,
                    "critical local readings should wait until the L4 lock expires");

  pyronet_risk_engine_tick(&engine, alert_ns + (2 * TEST_MINUTE_NS));
  test_expect_level(&engine, PYRONET_RISK_LEVEL_5,
                    "local sensor risk should apply after the neighbor-alert lock expires");
}

static void test_periodic_reporting_uses_fixed_interval(void)
{
  test_callbacks_t callbacks;
  pyronet_risk_engine_t engine = test_engine_init(&callbacks);

  pyronet_risk_engine_tick(&engine, 8 * TEST_HOUR_NS);
  test_expect_true(callbacks.periodic_report_count == 1,
                   "L1 should report every 8 hours");

  pyronet_risk_engine_submit_air_quality(&engine, 8 * TEST_HOUR_NS + 1, 36.0f, 35.0f, 120.0f);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_2,
                    "L2 thresholds should promote to level 2");

  pyronet_risk_engine_tick(&engine, (8 * TEST_HOUR_NS) + (4 * TEST_HOUR_NS));
  test_expect_true(callbacks.periodic_report_count == 1,
                   "risk level no longer shortens the periodic report interval");

  pyronet_risk_engine_tick(&engine, (16 * TEST_HOUR_NS) + 2);
  test_expect_true(callbacks.periodic_report_count == 2,
                   "fixed periodic report interval should remain 8 hours");
}

static void test_no_local_support_after_neighbor_alert_returns_to_l1(void)
{
  test_callbacks_t callbacks;
  pyronet_risk_engine_t engine = test_engine_init(&callbacks);

  pyronet_risk_engine_receive_neighbor_alert(&engine, 1);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_4,
                    "neighbor alert should lock risk to level 4");
  pyronet_risk_engine_tick(&engine, 30 * TEST_MINUTE_NS);
  test_expect_level(&engine, PYRONET_RISK_LEVEL_1,
                    "risk should return to L1 after the neighbor-alert lock expires");
}

int main(void)
{
  test_stale_config_is_rejected();
  test_threshold_and_l4_progression();
  test_l5_sensor_entry_sends_alerts();
  test_manual_override_does_not_alert();
  test_local_readings_demote_immediately();
  test_neighbor_alert_locks_risk_to_l4_for_two_minutes();
  test_periodic_reporting_uses_fixed_interval();
  test_no_local_support_after_neighbor_alert_returns_to_l1();
  puts("pyronet_risk_engine tests passed");
  return 0;
}
