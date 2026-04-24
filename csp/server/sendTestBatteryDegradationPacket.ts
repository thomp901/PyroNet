import {
  buildNodeUplinkEnvelope,
  buildSensorPayload,
  clamp,
  defaultApiPort,
  fetchFirstNodeId,
  nextUplinkId,
  postNodeUplink,
  toNumber,
} from "./testBackhaulUtils";

export {};

const batteryDegradationOpenThresholdPct = 20;
const batteryDegradationClearThresholdPct = 25;

async function main() {
  const apiBaseUrl = process.argv[4]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const nodeId = process.argv[2] ? clamp(Math.round(toNumber(process.argv[2], 1)), 1, 65_535) : await fetchFirstNodeId(apiBaseUrl);
  const batteryPct = clamp(Math.round(toNumber(process.argv[3], 18)), 0, 100);
  const gatewayId = clamp(Math.round(toNumber(process.argv[5], 9001)), 1, 65_535);
  const occurredAt = new Date();

  const payload = buildSensorPayload({
    packetType: 0x02,
    nodeId,
    occurredAt,
    riskLevel: 2,
    temperatureC: 27.4,
    humidityPct: 48.5,
    vocIaq: 155,
    pm25UgM3: 12.1,
    batteryPct,
  });

  const envelope = buildNodeUplinkEnvelope({
    gatewayId,
    uplinkId: nextUplinkId(),
    receivedAt: occurredAt,
    observedSrcIpv6: `2001:db8:100::${nodeId.toString(16)}`,
    payload,
  });

  const shouldOpen = batteryPct <= batteryDegradationOpenThresholdPct;
  const shouldClear = batteryPct >= batteryDegradationClearThresholdPct;

  console.info("[battery:packet] Sending 0x02 battery telemetry uplink", {
    nodeId,
    gatewayId,
    batteryPct,
    apiBaseUrl,
    occurredAt: occurredAt.toISOString(),
    expectedBehavior: shouldOpen
      ? "open battery degradation alert"
      : shouldClear
        ? "clear battery degradation alert"
        : "leave battery degradation alert state unchanged",
  });

  await postNodeUplink(apiBaseUrl, envelope);

  console.info("[battery:packet] Battery telemetry uplink accepted.");
  if (shouldOpen) {
    console.info("[battery:packet] If battery degradation email is enabled, you should now see a new delivery attempt.");
  } else if (shouldClear) {
    console.info("[battery:packet] Any open battery degradation alert for this node should now clear.");
  } else {
    console.info("[battery:packet] Battery is between open and clear thresholds, so alert state should stay unchanged.");
  }
}

main().catch((error) => {
  console.error("[battery:packet] Unable to send battery telemetry test packet", error);
  process.exitCode = 1;
});
