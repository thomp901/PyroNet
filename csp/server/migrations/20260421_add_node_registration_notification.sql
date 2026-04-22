DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1
    FROM pg_enum enum
    JOIN pg_type type ON type.oid = enum.enumtypid
    WHERE type.typname = 'notification_event_type'
      AND enum.enumlabel = 'node_registration'
  ) THEN
    ALTER TYPE notification_event_type ADD VALUE 'node_registration' AFTER 'critical_risk';
  END IF;
END $$;

INSERT INTO notification_preferences (recipient_id, event_type, email_enabled, sms_enabled, updated_at)
SELECT id, 'node_registration', FALSE, FALSE, NOW()
FROM notification_recipients
ON CONFLICT (recipient_id, event_type) DO NOTHING;
