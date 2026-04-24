import {
  buildGatewayRegistrationMessage,
  buildNodeUplinkEnvelope,
  clamp,
  defaultApiPort,
  nextUplinkId,
  postGatewayRegistration,
  postNodeUplink,
  toNumber,
  encodeFirmwareVersion,
  encodeIpv6,
} from "./testBackhaulUtils";

export {};

function buildRegistrationPayload({
  nodeId,
  latitude,
  longitude,
  firmwareVersion,
  batteryPct,
  parentIpv6,
}: {
  nodeId: number;
  latitude: number;
  longitude: number;
  firmwareVersion: string;
  batteryPct: number;
  parentIpv6: string | null;
}) {
  const buffer = Buffer.alloc(31);
  buffer.writeUInt8(0x01, 0);
  buffer.writeUInt8(0x01, 1);
  buffer.writeUInt16LE(nodeId, 2);
  buffer.writeFloatLE(latitude, 4);
  buffer.writeFloatLE(longitude, 8);
  buffer.writeUInt16LE(encodeFirmwareVersion(firmwareVersion), 12);
  buffer.writeUInt8(batteryPct, 14);
  encodeIpv6(parentIpv6).copy(buffer, 15);
  return buffer;
}

async function main() {
  const nodeId = clamp(Math.round(toNumber(process.argv[2], 25)), 1, 65_535);
  const apiBaseUrl = process.argv[3]?.trim() || `http://127.0.0.1:${defaultApiPort}`;
  const sourceIpv6 = process.argv[4]?.trim() || `2001:db8:100::${nodeId.toString(16)}`;
  const parentIpv6 = process.argv[5]?.trim() || "2001:db8:100::1";
  const gatewayId = clamp(Math.round(toNumber(process.argv[6], 9001)), 1, 65_535);
  const observedAt = new Date();

  const gatewayMessage = buildGatewayRegistrationMessage({
    gatewayId,
    reportedAt: observedAt,
    wisunIpv6: "2001:db8:200::1",
    latitude: 40.4237,
    longitude: -86.9212,
    softwareVersion: "3.1",
  });

  const payload = buildRegistrationPayload({
    nodeId,
    latitude: 40.4237,
    longitude: -86.9212,
    firmwareVersion: "3.1",
    batteryPct: 96,
    parentIpv6: parentIpv6.toLowerCase() === "none" ? null : parentIpv6,
  });

  const envelope = buildNodeUplinkEnvelope({
    gatewayId,
    uplinkId: nextUplinkId(),
    receivedAt: observedAt,
    observedSrcIpv6: sourceIpv6,
    payload,
  });

  console.info("[registration:packet] Sending gateway registration and 0x01 node registration", {
    nodeId,
    gatewayId,
    apiBaseUrl,
    sourceIpv6,
    parentIpv6: parentIpv6.toLowerCase() === "none" ? null : parentIpv6,
    observedAt: observedAt.toISOString(),
  });

  await postGatewayRegistration(apiBaseUrl, gatewayMessage);
  await postNodeUplink(apiBaseUrl, envelope);

  console.info("[registration:packet] Registration uplink accepted.");
  console.info("[registration:packet] If Node Join / Rejoin email is enabled, you should now see a new delivery attempt.");
}

main().catch((error) => {
  console.error("[registration:packet] Unable to send registration test packet", error);
  process.exitCode = 1;
});
