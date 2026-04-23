import { sendEmail } from "./email";

async function main() {
  const recipient = process.argv[2]?.trim();
  if (!recipient) {
    throw new Error("Usage: npm run test:email -- you@example.com");
  }

  const subject = process.argv[3]?.trim() || "PyroNet Mailtrap API test";
  const text =
    process.argv[4]?.trim() ||
    `This is a PyroNet Mailtrap API test email sent at ${new Date().toISOString()}.`;

  console.info("[email:test] Sending test email", {
    to: recipient,
    subject,
  });

  const result = await sendEmail({
    to: recipient,
    subject,
    text,
  });

  console.info("[email:test] Mailtrap API accepted test email", {
    to: recipient,
    providerMessageId: result.providerMessageId,
    acceptedRecipients: result.acceptedRecipients,
    rejectedRecipients: result.rejectedRecipients,
    response: result.response,
  });
}

main().catch((error) => {
  console.error("[email:test] Unable to send test email", error);
  process.exitCode = 1;
});
