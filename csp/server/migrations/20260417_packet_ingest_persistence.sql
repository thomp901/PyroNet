BEGIN;

DO $$
BEGIN
    CREATE TYPE packet_observation_status AS ENUM (
        'received',
        'pending',
        'sent',
        'acknowledged',
        'failed',
        'timed_out'
    );
EXCEPTION
    WHEN duplicate_object THEN NULL;
END;
$$;

ALTER TABLE device_registrations
    ADD COLUMN IF NOT EXISTS preferred_parent_ipv6 inet;

DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1
        FROM pg_constraint
        WHERE conname = 'chk_device_registrations_parent_ipv6_global_unicast'
    ) THEN
        ALTER TABLE device_registrations
            ADD CONSTRAINT chk_device_registrations_parent_ipv6_global_unicast CHECK (
                preferred_parent_ipv6 IS NULL OR (
                    family(preferred_parent_ipv6) = 6
                    AND NOT (preferred_parent_ipv6 <<= inet '::/128')
                    AND NOT (preferred_parent_ipv6 <<= inet '::1/128')
                    AND NOT (preferred_parent_ipv6 <<= inet 'fe80::/10')
                    AND NOT (preferred_parent_ipv6 <<= inet 'fc00::/7')
                    AND NOT (preferred_parent_ipv6 <<= inet 'ff00::/8')
                )
            );
    END IF;
END;
$$;

CREATE TABLE IF NOT EXISTS device_parent_updates (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    parent_ipv6 inet,
    parent_device_id bigint REFERENCES devices(id) ON DELETE SET NULL,
    observed_at timestamptz NOT NULL,
    ingested_at timestamptz NOT NULL DEFAULT NOW(),
    raw_payload_metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT chk_device_parent_updates_parent_ipv6_global_unicast CHECK (
        parent_ipv6 IS NULL OR (
            family(parent_ipv6) = 6
            AND NOT (parent_ipv6 <<= inet '::/128')
            AND NOT (parent_ipv6 <<= inet '::1/128')
            AND NOT (parent_ipv6 <<= inet 'fe80::/10')
            AND NOT (parent_ipv6 <<= inet 'fc00::/7')
            AND NOT (parent_ipv6 <<= inet 'ff00::/8')
        )
    ),
    CONSTRAINT chk_device_parent_updates_parent_not_self CHECK (
        parent_device_id IS NULL OR parent_device_id <> device_id
    ),
    CONSTRAINT chk_device_parent_updates_metadata_object CHECK (
        jsonb_typeof(raw_payload_metadata) = 'object'
    )
);

CREATE TABLE IF NOT EXISTS neighbor_alerts (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    source_device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    target_device_id bigint REFERENCES devices(id) ON DELETE SET NULL,
    risk_level smallint NOT NULL,
    occurred_at timestamptz NOT NULL,
    ingested_at timestamptz NOT NULL DEFAULT NOW(),
    status packet_observation_status NOT NULL DEFAULT 'received',
    raw_payload_metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT chk_neighbor_alerts_risk_level CHECK (risk_level BETWEEN 1 AND 5),
    CONSTRAINT chk_neighbor_alerts_target_not_self CHECK (
        target_device_id IS NULL OR target_device_id <> source_device_id
    ),
    CONSTRAINT chk_neighbor_alerts_metadata_object CHECK (
        jsonb_typeof(raw_payload_metadata) = 'object'
    )
);

CREATE INDEX IF NOT EXISTS idx_device_parent_updates_device_observed_at
    ON device_parent_updates (device_id, observed_at DESC);

CREATE INDEX IF NOT EXISTS idx_device_parent_updates_parent_device_observed_at
    ON device_parent_updates (parent_device_id, observed_at DESC);

CREATE INDEX IF NOT EXISTS idx_neighbor_alerts_source_occurred_at
    ON neighbor_alerts (source_device_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_neighbor_alerts_target_occurred_at
    ON neighbor_alerts (target_device_id, occurred_at DESC);

COMMENT ON TABLE device_parent_updates IS
'Append-only audit log of 0x08 preferred-parent changes reported by devices after registration.';

COMMENT ON TABLE neighbor_alerts IS
'Observed 0x07 lateral neighbor-alert packets, including the source device, optional target device, and captured delivery status.';

COMMENT ON COLUMN device_registrations.preferred_parent_ipv6 IS
'Preferred RPL parent observed at registration time from packet 0x01. NULL means the packet carried an all-zero parent value.';

COMMIT;
