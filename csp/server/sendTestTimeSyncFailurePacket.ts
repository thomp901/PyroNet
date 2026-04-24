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
  const minutesAgo = Math.max(16, Math.round(toNumber(process.argv[4], 20)));
  const gatewayId = clamp(Math.round(toNumber(process.argv[5], 9001)), 1, 65_535);
  const observedAt = new Date();

  const gatewayMessage = buildGatewayRegistrationMessage({
    gatewayId,
    reportedAt: observedAt,
    wisunIpv6: "2001:db8:200::1",
    latitude: 40.4237,
    longitude: -86.9212,
    softwareVersion: "3.1",
  });

  const routeUplink = buildNodeUplinkEnvelope({
    gatewayId,
    uplinkId: nextUplinkId(),
    receivedAt: observedAt,
    observedSrcIpv6: `2001:db8:100::${nodeId.toString(16)}`,
    payload: buildSensorPayload({
      packetType: 0x02,
      nodeId,
      occurredAt: observedAt,
      riskLevel: 1,
      temperatureC: 24.1,
      humidityPct: 41.2,
      vocIaq: 90,
      pm25UgM3: 7.4,
      batteryPct: 88,
    }),
  });

  console.info("[time-sync:packet] Queueing time sync, then backdating it to force timeout", {
    nodeId,
    gatewayId,
    apiBaseUrl,
    minutesAgo,
  });

  await postGatewayRegistration(apiBaseUrl, gatewayMessage);
  await postNodeUplink(apiBaseUrl, routeUplink);

  const queueResponse = await fetch(`${apiBaseUrl}/api/configuration/downlinks/time-sync`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify({ targetNodeIds: [nodeId] }),
  });
  const queueText = await queueResponse.text();
  if (!queueResponse.ok) {
    throw new Error(`Time sync queue failed with ${queueResponse.status}: ${queueText}`);
  }

  await withDbClient(async (client) => {
    await client.query(
      `
        WITH latest_pending AS (
          SELECT tse.id
          FROM time_sync_events tse
          JOIN devices d ON d.id = tse.device_id
          WHERE d.node_id = $2
            AND tse.status = 'pending'
          ORDER BY tse.id DESC
          LIMIT 1
        )
        UPDATE time_sync_events tse
        SET sent_at = NOW() - ($1 * interval '1 minute')
        FROM latest_pending
        WHERE tse.id = latest_pending.id
      `,
      [minutesAgo, nodeId],
    );
  });

  const notificationsResponse = await fetch(`${apiBaseUrl}/api/notifications`);
  if (!notificationsResponse.ok) {
    throw new Error(`Notification refresh failed with ${notificationsResponse.status}: ${await notificationsResponse.text()}`);
  }

  console.info("[time-sync:packet] Timeout scenario forced.");
  console.info("[time-sync:packet] If time sync failure email is enabled, you should now see a new delivery attempt.");
}

main().catch((error) => {
  console.error("[time-sync:packet] Unable to create time sync failure scenario", error);
  process.exitCode = 1;
});
