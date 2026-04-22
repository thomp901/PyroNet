BEGIN;

DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1
    FROM pg_enum enum
    JOIN pg_type type ON type.oid = enum.enumtypid
    WHERE type.typname = 'notification_event_type'
      AND enum.enumlabel = 'time_sync_failure'
  ) THEN
    ALTER TYPE notification_event_type ADD VALUE 'time_sync_failure' AFTER 'battery_degradation';
  END IF;
END;
$$;

DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1
    FROM pg_enum enum
    JOIN pg_type type ON type.oid = enum.enumtypid
    WHERE type.typname = 'notification_event_type'
      AND enum.enumlabel = 'nn_update_failure'
  ) THEN
    ALTER TYPE notification_event_type ADD VALUE 'nn_update_failure' AFTER 'time_sync_failure';
  END IF;
END;
$$;

DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1
    FROM pg_enum enum
    JOIN pg_type type ON type.oid = enum.enumtypid
    WHERE type.typname = 'notification_event_type'
      AND enum.enumlabel = 'config_update_failure'
  ) THEN
    ALTER TYPE notification_event_type ADD VALUE 'config_update_failure' AFTER 'nn_update_failure';
  END IF;
END;
$$;

INSERT INTO notification_preferences (recipient_id, event_type, email_enabled, sms_enabled, created_at, updated_at)
SELECT id, 'time_sync_failure', FALSE, FALSE, NOW(), NOW()
FROM notification_recipients
ON CONFLICT (recipient_id, event_type) DO NOTHING;

INSERT INTO notification_preferences (recipient_id, event_type, email_enabled, sms_enabled, created_at, updated_at)
SELECT id, 'nn_update_failure', FALSE, FALSE, NOW(), NOW()
FROM notification_recipients
ON CONFLICT (recipient_id, event_type) DO NOTHING;

INSERT INTO notification_preferences (recipient_id, event_type, email_enabled, sms_enabled, created_at, updated_at)
SELECT id, 'config_update_failure', FALSE, FALSE, NOW(), NOW()
FROM notification_recipients
ON CONFLICT (recipient_id, event_type) DO NOTHING;

COMMIT;
