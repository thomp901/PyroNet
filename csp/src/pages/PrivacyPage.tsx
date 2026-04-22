export function PrivacyPage() {
  return (
    <article className="card legal-document">
      <div className="legal-document-section">
        <p className="legal-document-updated">Effective date: April 20, 2026</p>
        <p className="legal-document-lead">
          PyroNet provides wildfire detection, environmental monitoring, and safety alerting tools for operators who
          need fast notice of hazardous conditions. This Privacy Policy explains how we collect, use, and protect
          personal information when users access the PyroNet dashboard, submit contact information, and enable SMS or
          email alerts.
        </p>
      </div>

      <section className="legal-document-section">
        <h2>Information we collect</h2>
        <p>We may collect the following categories of information:</p>
        <ul>
          <li>Name, organization name, email address, and phone number entered in the dashboard.</li>
          <li>Alert preferences, including whether a user enables SMS and email notifications.</li>
          <li>Operational telemetry, device status, incident data, and notification delivery history.</li>
          <li>Technical usage data needed to operate, secure, and troubleshoot the platform.</li>
        </ul>
      </section>

      <section className="legal-document-section">
        <h2>How we use information</h2>
        <p>PyroNet uses personal information to:</p>
        <ul>
          <li>Provide wildfire and environmental hazard alerts requested by the user.</li>
          <li>Deliver non-marketing incident, safety, and system notifications by SMS or email.</li>
          <li>Maintain account settings, contact preferences, and alert audit history.</li>
          <li>Monitor service reliability, investigate incidents, and improve system performance.</li>
        </ul>
      </section>

      <section className="legal-document-section">
        <h2>SMS consent and opt-in</h2>
        <p>
          Users opt in to SMS messaging by providing a mobile number in the PyroNet dashboard and enabling SMS alerts
          for one or more notification types. SMS messages are transactional safety alerts only and are not used for
          marketing or promotional campaigns.
        </p>
        <p>
          Message frequency varies based on device activity, wildfire conditions, environmental hazards, and account
          notification settings. Message and data rates may apply.
        </p>
      </section>

      <section className="legal-document-section">
        <h2>STOP and HELP</h2>
        <p>
          Users may reply <strong>STOP</strong> to opt out of SMS alerts at any time and <strong>HELP</strong> for
          assistance. Users may also update alert preferences directly in the PyroNet dashboard.
        </p>
      </section>

      <section className="legal-document-section">
        <h2>How we share information</h2>
        <p>
          We share information only as needed to operate the service, including with infrastructure, email, and SMS
          delivery providers such as Mailtrap and Twilio. We do not sell personal information or use SMS consent data
          for third-party marketing.
        </p>
      </section>

      <section className="legal-document-section">
        <h2>Data retention and security</h2>
        <p>
          PyroNet stores contact details, alert preferences, device telemetry, and notification delivery records for
          operational, compliance, and troubleshooting purposes. We use reasonable administrative, technical, and
          organizational safeguards to protect stored information.
        </p>
      </section>

      <section className="legal-document-section">
        <h2>Your choices</h2>
        <p>
          Users may update or disable notification settings through the dashboard. If a user wants account, privacy, or
          messaging support, they may contact PyroNet using the support process published by their deploying
          organization.
        </p>
      </section>
    </article>
  );
}
