import { sendEmail } from "./email";

function toNumber(value: string | undefined, fallback: number) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

async function main() {
  const recipient = process.argv[2]?.trim();
  if (!recipient) {
    throw new Error("Usage: npm run test:risk-notification -- you@example.com [nodeId] [riskLevel]");
  }

  const nodeId = Math.max(1, Math.round(toNumber(process.argv[3], 2)));
  const riskLevel = Math.max(1, Math.round(toNumber(process.argv[4], 5)));
  const temperatureC = 82.4;
  const humidityPct = 11.8;
  const vocIaq = 412;
  const pm25UgM3 = 96.3;

  const subject = `Critical risk detected at node ${nodeId}`;
  const text = `PyroNet detected a critical risk alert from node ${nodeId}. Risk ${riskLevel}. Temp ${temperatureC.toFixed(1)}C, RH ${humidityPct.toFixed(1)}%, VOC ${vocIaq}, PM2.5 ${pm25UgM3.toFixed(1)}.`;

  console.info("[risk:test] Sending critical risk notification", {
    to: recipient,
    nodeId,
    riskLevel,
    subject,
  });

  const result = await sendEmail({
    to: recipient,
    subject,
    text,
  });

  console.info("[risk:test] Mailtrap API accepted risk notification", {
    to: recipient,
    providerMessageId: result.providerMessageId,
    acceptedRecipients: result.acceptedRecipients,
    rejectedRecipients: result.rejectedRecipients,
    response: result.response,
  });
}

main().catch((error) => {
  console.error("[risk:test] Unable to send critical risk notification", error);
  process.exitCode = 1;
});
