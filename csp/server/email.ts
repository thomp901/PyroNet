import { MailtrapClient } from "mailtrap";

interface SendEmailInput {
  to: string;
  subject: string;
  text: string;
}

interface EmailSendResult {
  providerMessageId: string | null;
  acceptedRecipients: string[];
  rejectedRecipients: string[];
  response: string;
}

interface MailtrapConfig {
  token: string;
  from: string;
}

const senderName = "PyroNet Notifications";
const placeholderTokens = new Set(["<YOUR_API_TOKEN>", "YOUR_MAILTRAP_API_TOKEN", "your_real_mailtrap_api_token"]);

let cachedClient: MailtrapClient | null = null;
let cachedToken: string | null = null;

function readRequiredEnv(name: "MAILTRAP_TOKEN" | "EMAIL_FROM") {
  const value = process.env[name]?.trim();
  if (!value) {
    throw new Error(`${name} is not configured.`);
  }
  if (name === "MAILTRAP_TOKEN" && placeholderTokens.has(value)) {
    throw new Error(`${name} is still set to the example placeholder value.`);
  }
  return value;
}

function getMailtrapConfig(): MailtrapConfig {
  return {
    token: readRequiredEnv("MAILTRAP_TOKEN"),
    from: readRequiredEnv("EMAIL_FROM"),
  };
}

function getClient(config: MailtrapConfig) {
  if (!cachedClient || cachedToken !== config.token) {
    cachedClient = new MailtrapClient({
      token: config.token,
    });
    cachedToken = config.token;
  }

  return cachedClient;
}

export async function sendEmail({ to, subject, text }: SendEmailInput): Promise<EmailSendResult> {
  const config = getMailtrapConfig();
  const client = getClient(config);
  const result = await client.send({
    from: {
      email: config.from,
      name: senderName,
    },
    to: [{ email: to }],
    subject,
    text,
  });

  return {
    providerMessageId: result.message_ids[0] ?? null,
    acceptedRecipients: [to],
    rejectedRecipients: [],
    response: result.success ? "Mailtrap API accepted request." : "Mailtrap API rejected request.",
  };
}
