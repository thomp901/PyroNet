DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1
    FROM pg_enum enum
    JOIN pg_type type ON type.oid = enum.enumtypid
    WHERE type.typname = 'alert_type'
      AND enum.enumlabel = 'time_sync_failure'
  ) THEN
    ALTER TYPE alert_type ADD VALUE 'time_sync_failure' AFTER 'battery_degradation';
  END IF;
END $$;

DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1
    FROM pg_enum enum
    JOIN pg_type type ON type.oid = enum.enumtypid
    WHERE type.typname = 'alert_type'
      AND enum.enumlabel = 'nn_update_failure'
  ) THEN
    ALTER TYPE alert_type ADD VALUE 'nn_update_failure' AFTER 'time_sync_failure';
  END IF;
END $$;

DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1
    FROM pg_enum enum
    JOIN pg_type type ON type.oid = enum.enumtypid
    WHERE type.typname = 'alert_type'
      AND enum.enumlabel = 'config_update_failure'
  ) THEN
    ALTER TYPE alert_type ADD VALUE 'config_update_failure' AFTER 'nn_update_failure';
  END IF;
END $$;
