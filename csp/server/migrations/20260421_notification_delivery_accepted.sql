DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1
        FROM pg_type type_def
        JOIN pg_enum enum_def ON enum_def.enumtypid = type_def.oid
        WHERE type_def.typname = 'notification_delivery_status'
          AND enum_def.enumlabel = 'accepted'
    ) THEN
        ALTER TYPE notification_delivery_status ADD VALUE 'accepted' AFTER 'queued';
    END IF;
END $$;

UPDATE notification_deliveries
SET status = 'accepted'
WHERE status = 'sent'
  AND delivered_at IS NULL;
