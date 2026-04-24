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

async function main() {
  const apiBaseUrl = process.argv[4]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const nodeId = process.argv[2] ? clamp(Math.round(toNumber(process.argv[2], 1)), 1, 65_535) : await fetchFirstNodeId(apiBaseUrl);
  const riskLevel = clamp(Math.round(toNumber(process.argv[3], 5)), 1, 5);
  const gatewayId = clamp(Math.round(toNumber(process.argv[5], 9001)), 1, 65_535);
  const occurredAt = new Date();

  const payload = buildSensorPayload({
    packetType: 0x03,
    nodeId,
    occurredAt,
    riskLevel,
    temperatureC: 82.4,
    humidityPct: 11.8,
    vocIaq: 412,
    pm25UgM3: 96.3,
    batteryPct: 74,
  });

  const envelope = buildNodeUplinkEnvelope({
    gatewayId,
    uplinkId: nextUplinkId(),
    receivedAt: occurredAt,
    observedSrcIpv6: `2001:db8:100::${nodeId.toString(16)}`,
    payload,
  });

  console.info("[risk:packet] Sending 0x03 critical-risk uplink", {
    nodeId,
    gatewayId,
    riskLevel,
    apiBaseUrl,
    occurredAt: occurredAt.toISOString(),
  });

  await postNodeUplink(apiBaseUrl, envelope);

  console.info("[risk:packet] Critical-risk uplink accepted.");
  console.info("[risk:packet] If critical-risk email is enabled, you should now see a new delivery attempt.");
}

main().catch((error) => {
  console.error("[risk:packet] Unable to send critical-risk test packet", error);
  process.exitCode = 1;
});
