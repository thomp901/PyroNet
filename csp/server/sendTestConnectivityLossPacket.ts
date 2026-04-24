import {
  buildGatewayRegistrationMessage,
  buildNodeUplinkEnvelope,
  buildSensorPayload,
  clamp,
  defaultApiPort,
  fetchFirstNodeId,
  nextUplinkId,
  postGatewayRegistration,
  postNodeUplink,
  toNumber,
  withDbClient,
} from "./testBackhaulUtils";

export {};

async function main() {
  const apiBaseUrl = process.argv[3]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const nodeId = process.argv[2] ? clamp(Math.round(toNumber(process.argv[2], 1)), 1, 65_535) : await fetchFirstNodeId(apiBaseUrl);
  const minutesAgo = Math.max(6, Math.round(toNumber(process.argv[4], 6)));
  const gatewayId = clamp(Math.round(toNumber(process.argv[5], 9001)), 1, 65_535);
  const receivedAt = new Date(Date.now() - minutesAgo * 60 * 1000);

  const gatewayMessage = buildGatewayRegistrationMessage({
    gatewayId,
    reportedAt: receivedAt,
    wisunIpv6: "2001:db8:200::1",
    latitude: 40.4237,
    longitude: -86.9212,
    softwareVersion: "3.1",
  });

  const payload = buildSensorPayload({
    packetType: 0x02,
    nodeId,
    occurredAt: receivedAt,
    riskLevel: 1,
    temperatureC: 24.2,
    humidityPct: 40.1,
    vocIaq: 100,
    pm25UgM3: 8.1,
    batteryPct: 82,
  });

  const envelope = buildNodeUplinkEnvelope({
    gatewayId,
    uplinkId: nextUplinkId(),
    receivedAt,
    observedSrcIpv6: `2001:db8:100::${nodeId.toString(16)}`,
    payload,
  });

  console.info("[connectivity:packet] Sending stale benign uplink to create offline scenario", {
    nodeId,
    gatewayId,
    apiBaseUrl,
    minutesAgo,
    receivedAt: receivedAt.toISOString(),
  });

  await postGatewayRegistration(apiBaseUrl, gatewayMessage);
  await postNodeUplink(apiBaseUrl, envelope);

  await withDbClient(async (client) => {
    await client.query(
      `
        UPDATE devices
        SET
          first_registered_at = LEAST(first_registered_at, NOW() - ($1 * interval '1 minute')),
          last_registered_at = LEAST(last_registered_at, NOW() - ($1 * interval '1 minute')),
          last_seen_at = NOW() - ($1 * interval '1 minute'),
          last_observed_gateway_at = NOW() - ($1 * interval '1 minute')
        WHERE node_id = $2
      `,
      [minutesAgo, nodeId],
    );
  });

  await fetch(`${apiBaseUrl}/api/notifications`);

  console.info("[connectivity:packet] Offline scenario accepted.");
  console.info("[connectivity:packet] If connectivity-loss email is enabled, you should now see a new delivery attempt.");
}

main().catch((error) => {
  console.error("[connectivity:packet] Unable to create connectivity-loss test scenario", error);
  process.exitCode = 1;
});
