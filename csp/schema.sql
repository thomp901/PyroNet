BEGIN;

CREATE TYPE sensor_reading_source AS ENUM (
    'periodic_report',
    'critical_alert'
);

CREATE TYPE alert_type AS ENUM (
    'critical_risk',
    'connectivity_loss',
    'battery_degradation',
    'time_sync_failure',
    'nn_update_failure',
    'config_update_failure',
    'system'
);

CREATE TYPE alert_status AS ENUM (
    'open',
    'acknowledged',
    'cleared'
);

CREATE TYPE alert_severity AS ENUM (
    'info',
    'warning',
    'critical'
);

CREATE TYPE alert_event_type AS ENUM (
    'opened',
    'acknowledged',
    'cleared',
    'reopened'
);

CREATE TYPE nn_revision_source AS ENUM (
    'automatic',
    'manual',
    'imported'
);

CREATE TYPE nn_distribution_status AS ENUM (
    'pending',
    'sent',
    'acknowledged',
    'failed',
    'timed_out'
);

CREATE TYPE time_sync_status AS ENUM (
    'pending',
    'sent',
    'acknowledged',
    'failed',
    'timed_out'
);

CREATE TYPE config_deployment_status AS ENUM (
    'pending',
    'sent',
    'acknowledged',
    'failed',
    'timed_out'
);

CREATE TYPE notification_event_type AS ENUM (
    'critical_risk',
    'node_registration',
    'connectivity_loss',
    'battery_degradation',
    'time_sync_failure',
    'nn_update_failure',
    'config_update_failure'
);

CREATE TYPE notification_delivery_status AS ENUM (
    'queued',
    'accepted',
    'sent',
    'failed',
    'skipped'
);

CREATE TYPE gateway_uplink_storage_status AS ENUM (
    'received',
    'projected',
    'rejected'
);

CREATE TYPE gateway_uplink_receipt_status AS ENUM (
    'durable_ingest',
    'permanent_reject'
);

CREATE TYPE gateway_downlink_status AS ENUM (
    'pending',
    'dispatched',
    'delivered',
    'unknown_node',
    'mesh_delivery_failed',
    'permanent_reject',
    'transport_failed'
);

CREATE TYPE device_parent_observation_source AS ENUM (
    'registration',
    'parent_update'
);

CREATE OR REPLACE FUNCTION set_updated_at()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$;

CREATE SEQUENCE config_revisions_config_id_seq
    AS bigint
    START WITH 1
    INCREMENT BY 1
    MINVALUE 1
    NO MAXVALUE
    CACHE 1;

CREATE TABLE config_revisions (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    config_id bigint NOT NULL DEFAULT nextval('config_revisions_config_id_seq'::regclass),
    l2_temp_thresh numeric(6,2) NOT NULL,
    l2_humidity_thresh numeric(5,2) NOT NULL,
    l2_voc_thresh integer NOT NULL,
    l3_temp_thresh numeric(6,2) NOT NULL,
    l3_humidity_thresh numeric(5,2) NOT NULL,
    l3_voc_thresh integer NOT NULL,
    l4_voc_thresh integer NOT NULL,
    l5_voc_thresh integer NOT NULL,
    l5_pm25_thresh numeric(8,1) NOT NULL,
    activated_at timestamptz,
    retired_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    updated_at timestamptz NOT NULL DEFAULT NOW(),
    notes text,
    metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT uq_config_revisions_config_id UNIQUE (config_id),
    CONSTRAINT chk_config_revisions_config_id_positive CHECK (config_id > 0),
    CONSTRAINT chk_config_revisions_l2_humidity CHECK (l2_humidity_thresh >= 0 AND l2_humidity_thresh <= 100),
    CONSTRAINT chk_config_revisions_l3_humidity CHECK (l3_humidity_thresh >= 0 AND l3_humidity_thresh <= 100),
    CONSTRAINT chk_config_revisions_l2_voc CHECK (l2_voc_thresh >= 0),
    CONSTRAINT chk_config_revisions_l3_voc CHECK (l3_voc_thresh >= 0),
    CONSTRAINT chk_config_revisions_l4_voc CHECK (l4_voc_thresh >= 0),
    CONSTRAINT chk_config_revisions_l5_voc CHECK (l5_voc_thresh >= 0),
    CONSTRAINT chk_config_revisions_l5_pm25 CHECK (l5_pm25_thresh >= 0),
    CONSTRAINT chk_config_revisions_retired_after_activated CHECK (retired_at IS NULL OR activated_at IS NULL OR retired_at > activated_at),
    CONSTRAINT chk_config_revisions_metadata_object CHECK (jsonb_typeof(metadata) = 'object')
);

ALTER SEQUENCE config_revisions_config_id_seq
    OWNED BY config_revisions.config_id;

CREATE TABLE gateways (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    gateway_id integer NOT NULL,
    current_latitude numeric(9,6),
    current_longitude numeric(9,6),
    current_sw_version_packed integer,
    first_registered_at timestamptz,
    last_registered_at timestamptz,
    last_gateway_timestamp_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    updated_at timestamptz NOT NULL DEFAULT NOW(),
    metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT uq_gateways_gateway_id UNIQUE (gateway_id),
    CONSTRAINT chk_gateways_gateway_id_uint16 CHECK (
        gateway_id >= 0 AND gateway_id <= 65535
    ),
    CONSTRAINT chk_gateways_latitude CHECK (
        current_latitude >= -90 AND current_latitude <= 90
    ),
    CONSTRAINT chk_gateways_longitude CHECK (
        current_longitude >= -180 AND current_longitude <= 180
    ),
    CONSTRAINT chk_gateways_sw_version_uint16 CHECK (
        current_sw_version_packed IS NULL OR (
            current_sw_version_packed >= 0 AND current_sw_version_packed <= 65535
        )
    ),
    CONSTRAINT chk_gateways_registration_window CHECK (
        first_registered_at IS NULL
        OR last_registered_at IS NULL
        OR last_registered_at >= first_registered_at
    ),
    CONSTRAINT chk_gateways_registration_fields_present_together CHECK (
        (first_registered_at IS NULL) = (last_registered_at IS NULL)
    ),
    CONSTRAINT chk_gateways_current_snapshot_presence CHECK (
        (current_latitude IS NULL AND current_longitude IS NULL AND current_sw_version_packed IS NULL)
        OR (
            current_latitude IS NOT NULL
            AND current_longitude IS NOT NULL
            AND current_sw_version_packed IS NOT NULL
        )
    ),
    CONSTRAINT chk_gateways_metadata_object CHECK (
        jsonb_typeof(metadata) = 'object'
    )
);

CREATE TABLE gateway_registrations (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    gateway_row_id bigint NOT NULL REFERENCES gateways(id) ON DELETE CASCADE,
    reported_at timestamptz NOT NULL,
    ingested_at timestamptz NOT NULL DEFAULT NOW(),
    latitude numeric(9,6) NOT NULL,
    longitude numeric(9,6) NOT NULL,
    sw_version_packed integer NOT NULL,
    backhaul_version smallint NOT NULL,
    raw_message bytea NOT NULL,
    raw_payload_metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT chk_gateway_registrations_latitude CHECK (
        latitude >= -90 AND latitude <= 90
    ),
    CONSTRAINT chk_gateway_registrations_longitude CHECK (
        longitude >= -180 AND longitude <= 180
    ),
    CONSTRAINT chk_gateway_registrations_sw_version_uint16 CHECK (
        sw_version_packed >= 0 AND sw_version_packed <= 65535
    ),
    CONSTRAINT chk_gateway_registrations_backhaul_version_uint8 CHECK (
        backhaul_version >= 0 AND backhaul_version <= 255
    ),
    CONSTRAINT chk_gateway_registrations_raw_message_length CHECK (
        octet_length(raw_message) = 18
    ),
    CONSTRAINT chk_gateway_registrations_metadata_object CHECK (
        jsonb_typeof(raw_payload_metadata) = 'object'
    )
);

CREATE TABLE gateway_uplinks (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    gateway_row_id bigint NOT NULL REFERENCES gateways(id) ON DELETE CASCADE,
    uplink_id numeric(20,0) NOT NULL,
    received_at timestamptz,
    observed_src_ipv6 inet,
    backhaul_version smallint,
    payload_len integer NOT NULL,
    payload_type smallint,
    payload_version smallint,
    payload bytea,
    raw_envelope bytea NOT NULL,
    storage_status gateway_uplink_storage_status NOT NULL DEFAULT 'received',
    first_csp_received_at timestamptz NOT NULL DEFAULT NOW(),
    last_csp_received_at timestamptz NOT NULL DEFAULT NOW(),
    delivery_attempt_count integer NOT NULL DEFAULT 1,
    metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT uq_gateway_uplinks_gateway_uplink UNIQUE (gateway_row_id, uplink_id),
    CONSTRAINT chk_gateway_uplinks_uplink_id_uint64 CHECK (
        uplink_id >= 0 AND uplink_id <= 18446744073709551615
    ),
    CONSTRAINT chk_gateway_uplinks_ipv6_global_unicast CHECK (
        observed_src_ipv6 IS NULL OR (
            family(observed_src_ipv6) = 6
            AND NOT (observed_src_ipv6 <<= inet '::/128')
            AND NOT (observed_src_ipv6 <<= inet '::1/128')
            AND NOT (observed_src_ipv6 <<= inet 'ff00::/8')
        )
    ),
    CONSTRAINT chk_gateway_uplinks_backhaul_version_uint8 CHECK (
        backhaul_version IS NULL OR (
            backhaul_version >= 0 AND backhaul_version <= 255
        )
    ),
    CONSTRAINT chk_gateway_uplinks_payload_len_non_negative CHECK (
        payload_len >= 0
    ),
    CONSTRAINT chk_gateway_uplinks_payload_type_uint8 CHECK (
        payload_type IS NULL OR (payload_type >= 0 AND payload_type <= 255)
    ),
    CONSTRAINT chk_gateway_uplinks_payload_version_uint8 CHECK (
        payload_version IS NULL OR (payload_version >= 0 AND payload_version <= 255)
    ),
    CONSTRAINT chk_gateway_uplinks_payload_octet_length CHECK (
        payload IS NULL OR octet_length(payload) = payload_len
    ),
    CONSTRAINT chk_gateway_uplinks_raw_envelope_present CHECK (
        octet_length(raw_envelope) >= payload_len
    ),
    CONSTRAINT chk_gateway_uplinks_attempt_count_positive CHECK (
        delivery_attempt_count > 0
    ),
    CONSTRAINT chk_gateway_uplinks_last_received_after_first CHECK (
        last_csp_received_at >= first_csp_received_at
    ),
    CONSTRAINT chk_gateway_uplinks_received_at_required_when_not_rejected CHECK (
        storage_status = 'rejected' OR received_at IS NOT NULL
    ),
    CONSTRAINT chk_gateway_uplinks_payload_required_when_not_rejected CHECK (
        storage_status = 'rejected' OR payload IS NOT NULL
    ),
    CONSTRAINT chk_gateway_uplinks_observed_src_ipv6_required_when_not_rejected CHECK (
        storage_status = 'rejected' OR observed_src_ipv6 IS NOT NULL
    ),
    CONSTRAINT chk_gateway_uplinks_backhaul_version_required_when_not_rejected CHECK (
        storage_status = 'rejected' OR backhaul_version IS NOT NULL
    ),
    CONSTRAINT chk_gateway_uplinks_metadata_object CHECK (
        jsonb_typeof(metadata) = 'object'
    )
);

CREATE TABLE gateway_uplink_receipts (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    gateway_uplink_id bigint NOT NULL REFERENCES gateway_uplinks(id) ON DELETE CASCADE,
    status gateway_uplink_receipt_status NOT NULL,
    receipt_version smallint NOT NULL,
    responded_at timestamptz NOT NULL DEFAULT NOW(),
    receipt_payload bytea,
    reject_code text,
    reject_detail text,
    metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT uq_gateway_uplink_receipts_uplink UNIQUE (gateway_uplink_id),
    CONSTRAINT chk_gateway_uplink_receipts_version_uint8 CHECK (
        receipt_version >= 0 AND receipt_version <= 255
    ),
    CONSTRAINT chk_gateway_uplink_receipts_payload_length CHECK (
        receipt_payload IS NULL OR octet_length(receipt_payload) = 13
    ),
    CONSTRAINT chk_gateway_uplink_receipts_reject_reason_required CHECK (
        status <> 'permanent_reject' OR reject_code IS NOT NULL
    ),
    CONSTRAINT chk_gateway_uplink_receipts_metadata_object CHECK (
        jsonb_typeof(metadata) = 'object'
    )
);

CREATE TABLE gateway_uplink_dead_letters (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    gateway_uplink_id bigint NOT NULL REFERENCES gateway_uplinks(id) ON DELETE CASCADE,
    gateway_uplink_receipt_id bigint REFERENCES gateway_uplink_receipts(id) ON DELETE SET NULL,
    dead_lettered_at timestamptz NOT NULL DEFAULT NOW(),
    reason_code text NOT NULL,
    reason_detail text,
    operator_note text,
    metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT uq_gateway_uplink_dead_letters_uplink UNIQUE (gateway_uplink_id),
    CONSTRAINT uq_gateway_uplink_dead_letters_receipt UNIQUE (gateway_uplink_receipt_id),
    CONSTRAINT chk_gateway_uplink_dead_letters_reason_not_blank CHECK (
        length(btrim(reason_code)) > 0
    ),
    CONSTRAINT chk_gateway_uplink_dead_letters_metadata_object CHECK (
        jsonb_typeof(metadata) = 'object'
    )
);

CREATE SEQUENCE gateway_downlink_downlink_id_seq
    AS bigint
    START WITH 1
    INCREMENT BY 1
    MINVALUE 1
    NO MAXVALUE
    CACHE 1;

CREATE TABLE devices (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    node_id smallint NOT NULL,
    current_ipv6 inet,
    current_parent_ipv6 inet,
    last_observed_gateway_row_id bigint REFERENCES gateways(id) ON DELETE SET NULL,
    last_observed_gateway_at timestamptz,
    current_latitude numeric(9,6) NOT NULL,
    current_longitude numeric(9,6) NOT NULL,
    current_firmware_version text,
    first_registered_at timestamptz NOT NULL DEFAULT NOW(),
    last_registered_at timestamptz NOT NULL DEFAULT NOW(),
    last_seen_at timestamptz,
    latest_reported_at timestamptz,
    latest_risk_level smallint,
    latest_temperature_c numeric(6,2),
    latest_humidity_pct numeric(5,2),
    latest_voc_iaq integer,
    latest_pm25_ug_m3 numeric(8,1),
    latest_battery_pct smallint,
    latest_pressure_hpa numeric(8,2),
    latest_battery_health_score smallint,
    current_config_revision_id bigint REFERENCES config_revisions(id) ON DELETE SET NULL,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    updated_at timestamptz NOT NULL DEFAULT NOW(),
    CONSTRAINT uq_devices_node_id UNIQUE (node_id),
    CONSTRAINT uq_devices_current_ipv6 UNIQUE (current_ipv6),
    CONSTRAINT chk_devices_node_id_nonnegative CHECK (node_id >= 0),
    CONSTRAINT chk_devices_current_latitude CHECK (current_latitude >= -90 AND current_latitude <= 90),
    CONSTRAINT chk_devices_current_longitude CHECK (current_longitude >= -180 AND current_longitude <= 180),
    CONSTRAINT chk_devices_current_ipv6_global_unicast CHECK (
        current_ipv6 IS NULL OR (
            family(current_ipv6) = 6
            AND NOT (current_ipv6 <<= inet '::/128')
            AND NOT (current_ipv6 <<= inet '::1/128')
            AND NOT (current_ipv6 <<= inet 'ff00::/8')
        )
    ),
    CONSTRAINT chk_devices_current_parent_ipv6_global_unicast CHECK (
        current_parent_ipv6 IS NULL OR (
            family(current_parent_ipv6) = 6
            AND NOT (current_parent_ipv6 <<= inet '::/128')
            AND NOT (current_parent_ipv6 <<= inet '::1/128')
            AND NOT (current_parent_ipv6 <<= inet 'ff00::/8')
        )
    ),
    CONSTRAINT chk_devices_latest_risk_level CHECK (latest_risk_level IS NULL OR latest_risk_level BETWEEN 1 AND 5),
    CONSTRAINT chk_devices_latest_humidity CHECK (latest_humidity_pct IS NULL OR (latest_humidity_pct >= 0 AND latest_humidity_pct <= 100)),
    CONSTRAINT chk_devices_latest_voc CHECK (latest_voc_iaq IS NULL OR latest_voc_iaq >= 0),
    CONSTRAINT chk_devices_latest_pm25 CHECK (latest_pm25_ug_m3 IS NULL OR latest_pm25_ug_m3 >= 0),
    CONSTRAINT chk_devices_latest_battery CHECK (latest_battery_pct IS NULL OR (latest_battery_pct >= 0 AND latest_battery_pct <= 100)),
    CONSTRAINT chk_devices_latest_pressure CHECK (latest_pressure_hpa IS NULL OR latest_pressure_hpa > 0),
    CONSTRAINT chk_devices_latest_battery_health CHECK (
        latest_battery_health_score IS NULL OR (
            latest_battery_health_score >= 0 AND latest_battery_health_score <= 100
        )
    ),
    CONSTRAINT chk_devices_last_observed_gateway_presence CHECK (
        (last_observed_gateway_row_id IS NULL) = (last_observed_gateway_at IS NULL)
    ),
    CONSTRAINT chk_devices_seen_after_registration CHECK (
        last_seen_at IS NULL OR last_seen_at >= first_registered_at
    )
);

CREATE TABLE device_registrations (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    gateway_uplink_id bigint UNIQUE REFERENCES gateway_uplinks(id) ON DELETE RESTRICT,
    observed_ipv6 inet NOT NULL,
    latitude numeric(9,6) NOT NULL,
    longitude numeric(9,6) NOT NULL,
    firmware_version text,
    battery_pct smallint,
    observed_at timestamptz NOT NULL,
    ingested_at timestamptz NOT NULL DEFAULT NOW(),
    raw_payload_metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT chk_device_registrations_ipv6_global_unicast CHECK (
        family(observed_ipv6) = 6
        AND NOT (observed_ipv6 <<= inet '::/128')
        AND NOT (observed_ipv6 <<= inet '::1/128')
        AND NOT (observed_ipv6 <<= inet 'ff00::/8')
    ),
    CONSTRAINT chk_device_registrations_latitude CHECK (latitude >= -90 AND latitude <= 90),
    CONSTRAINT chk_device_registrations_longitude CHECK (longitude >= -180 AND longitude <= 180),
    CONSTRAINT chk_device_registrations_battery CHECK (battery_pct IS NULL OR (battery_pct >= 0 AND battery_pct <= 100)),
    CONSTRAINT chk_device_registrations_ingested_after_observed CHECK (ingested_at >= observed_at),
    CONSTRAINT chk_device_registrations_metadata_object CHECK (jsonb_typeof(raw_payload_metadata) = 'object')
);

CREATE TABLE device_ipv6_history (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    ipv6_address inet NOT NULL,
    registration_id bigint REFERENCES device_registrations(id) ON DELETE SET NULL,
    valid_from timestamptz NOT NULL,
    valid_to timestamptz,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    CONSTRAINT chk_device_ipv6_history_ipv6_global_unicast CHECK (
        family(ipv6_address) = 6
        AND NOT (ipv6_address <<= inet '::/128')
        AND NOT (ipv6_address <<= inet '::1/128')
        AND NOT (ipv6_address <<= inet 'ff00::/8')
    ),
    CONSTRAINT chk_device_ipv6_history_valid_window CHECK (valid_to IS NULL OR valid_to > valid_from)
);

CREATE TABLE device_parent_observations (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    gateway_uplink_id bigint UNIQUE NOT NULL REFERENCES gateway_uplinks(id) ON DELETE RESTRICT,
    source_type device_parent_observation_source NOT NULL,
    observed_parent_ipv6 inet,
    observed_at timestamptz NOT NULL,
    ingested_at timestamptz NOT NULL DEFAULT NOW(),
    raw_payload_metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT chk_device_parent_observations_ipv6_global_unicast CHECK (
        observed_parent_ipv6 IS NULL OR (
            family(observed_parent_ipv6) = 6
            AND NOT (observed_parent_ipv6 <<= inet '::/128')
            AND NOT (observed_parent_ipv6 <<= inet '::1/128')
            AND NOT (observed_parent_ipv6 <<= inet 'ff00::/8')
        )
    ),
    CONSTRAINT chk_device_parent_observations_metadata_object CHECK (
        jsonb_typeof(raw_payload_metadata) = 'object'
    )
);

CREATE TABLE sensor_readings (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    gateway_uplink_id bigint UNIQUE REFERENCES gateway_uplinks(id) ON DELETE RESTRICT,
    source_type sensor_reading_source NOT NULL,
    reported_at timestamptz NOT NULL,
    ingested_at timestamptz NOT NULL DEFAULT NOW(),
    risk_level smallint NOT NULL,
    temperature_c numeric(6,2) NOT NULL,
    humidity_pct numeric(5,2) NOT NULL,
    voc_iaq integer NOT NULL,
    pm25_ug_m3 numeric(8,1) NOT NULL,
    battery_pct smallint,
    pressure_hpa numeric(8,2),
    battery_health_score smallint,
    config_revision_id bigint REFERENCES config_revisions(id) ON DELETE SET NULL,
    raw_payload_metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT chk_sensor_readings_risk_level CHECK (risk_level BETWEEN 1 AND 5),
    CONSTRAINT chk_sensor_readings_humidity CHECK (humidity_pct >= 0 AND humidity_pct <= 100),
    CONSTRAINT chk_sensor_readings_voc CHECK (voc_iaq >= 0),
    CONSTRAINT chk_sensor_readings_pm25 CHECK (pm25_ug_m3 >= 0),
    CONSTRAINT chk_sensor_readings_battery CHECK (battery_pct IS NULL OR (battery_pct >= 0 AND battery_pct <= 100)),
    CONSTRAINT chk_sensor_readings_pressure CHECK (pressure_hpa IS NULL OR pressure_hpa > 0),
    CONSTRAINT chk_sensor_readings_battery_health CHECK (
        battery_health_score IS NULL OR (
            battery_health_score >= 0 AND battery_health_score <= 100
        )
    ),
    CONSTRAINT chk_sensor_readings_metadata_object CHECK (jsonb_typeof(raw_payload_metadata) = 'object')
);

CREATE TABLE alerts (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint REFERENCES devices(id) ON DELETE CASCADE,
    sensor_reading_id bigint REFERENCES sensor_readings(id) ON DELETE SET NULL,
    config_revision_id bigint REFERENCES config_revisions(id) ON DELETE SET NULL,
    alert_type alert_type NOT NULL,
    status alert_status NOT NULL DEFAULT 'open',
    severity alert_severity NOT NULL,
    title text NOT NULL,
    details jsonb NOT NULL DEFAULT '{}'::jsonb,
    occurred_at timestamptz NOT NULL,
    detected_at timestamptz NOT NULL DEFAULT NOW(),
    acknowledged_at timestamptz,
    acknowledged_by text,
    cleared_at timestamptz,
    cleared_by text,
    latest_event_at timestamptz NOT NULL DEFAULT NOW(),
    snapshot_reported_at timestamptz,
    snapshot_risk_level smallint,
    snapshot_temperature_c numeric(6,2),
    snapshot_humidity_pct numeric(5,2),
    snapshot_voc_iaq integer,
    snapshot_pm25_ug_m3 numeric(8,1),
    snapshot_battery_pct smallint,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    updated_at timestamptz NOT NULL DEFAULT NOW(),
    CONSTRAINT chk_alerts_title_not_blank CHECK (length(btrim(title)) > 0),
    CONSTRAINT chk_alerts_details_object CHECK (jsonb_typeof(details) = 'object'),
    CONSTRAINT chk_alerts_ack_timestamp CHECK (acknowledged_at IS NULL OR acknowledged_at >= detected_at),
    CONSTRAINT chk_alerts_clear_timestamp CHECK (cleared_at IS NULL OR cleared_at >= detected_at),
    CONSTRAINT chk_alerts_clear_after_ack CHECK (cleared_at IS NULL OR acknowledged_at IS NULL OR cleared_at >= acknowledged_at),
    CONSTRAINT chk_alerts_ack_required_when_acknowledged CHECK (
        status <> 'acknowledged' OR acknowledged_at IS NOT NULL
    ),
    CONSTRAINT chk_alerts_clear_required_when_cleared CHECK (
        status <> 'cleared' OR cleared_at IS NOT NULL
    ),
    CONSTRAINT chk_alerts_latest_event_at CHECK (latest_event_at >= detected_at),
    CONSTRAINT chk_alerts_snapshot_risk CHECK (
        snapshot_risk_level IS NULL OR snapshot_risk_level BETWEEN 1 AND 5
    ),
    CONSTRAINT chk_alerts_snapshot_humidity CHECK (
        snapshot_humidity_pct IS NULL OR (
            snapshot_humidity_pct >= 0 AND snapshot_humidity_pct <= 100
        )
    ),
    CONSTRAINT chk_alerts_snapshot_voc CHECK (snapshot_voc_iaq IS NULL OR snapshot_voc_iaq >= 0),
    CONSTRAINT chk_alerts_snapshot_pm25 CHECK (snapshot_pm25_ug_m3 IS NULL OR snapshot_pm25_ug_m3 >= 0),
    CONSTRAINT chk_alerts_snapshot_battery CHECK (
        snapshot_battery_pct IS NULL OR (
            snapshot_battery_pct >= 0 AND snapshot_battery_pct <= 100
        )
    )
);

CREATE TABLE alert_events (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    alert_id bigint NOT NULL REFERENCES alerts(id) ON DELETE CASCADE,
    event_type alert_event_type NOT NULL,
    previous_status alert_status,
    new_status alert_status,
    event_at timestamptz NOT NULL DEFAULT NOW(),
    actor text,
    note text,
    details jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT chk_alert_events_details_object CHECK (jsonb_typeof(details) = 'object')
);

CREATE TABLE nn_revisions (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    revision_no integer NOT NULL,
    radius_meters integer NOT NULL,
    revision_source nn_revision_source NOT NULL DEFAULT 'automatic',
    selection_basis_at timestamptz,
    active_from timestamptz NOT NULL DEFAULT NOW(),
    active_to timestamptz,
    details jsonb NOT NULL DEFAULT '{}'::jsonb,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    updated_at timestamptz NOT NULL DEFAULT NOW(),
    CONSTRAINT uq_nn_revisions_device_revision UNIQUE (device_id, revision_no),
    CONSTRAINT uq_nn_revisions_id_device UNIQUE (id, device_id),
    CONSTRAINT chk_nn_revisions_revision_no_positive CHECK (revision_no > 0),
    CONSTRAINT chk_nn_revisions_radius_positive CHECK (radius_meters > 0),
    CONSTRAINT chk_nn_revisions_active_window CHECK (active_to IS NULL OR active_to > active_from),
    CONSTRAINT chk_nn_revisions_details_object CHECK (jsonb_typeof(details) = 'object')
);

CREATE TABLE nn_revision_memberships (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    nn_revision_id bigint NOT NULL REFERENCES nn_revisions(id) ON DELETE CASCADE,
    owner_device_id bigint NOT NULL,
    neighbor_device_id bigint NOT NULL REFERENCES devices(id) ON DELETE RESTRICT,
    neighbor_rank smallint NOT NULL,
    distance_meters integer NOT NULL,
    neighbor_latitude numeric(9,6) NOT NULL,
    neighbor_longitude numeric(9,6) NOT NULL,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    CONSTRAINT uq_nn_revision_memberships_neighbor UNIQUE (nn_revision_id, neighbor_device_id),
    CONSTRAINT uq_nn_revision_memberships_rank UNIQUE (nn_revision_id, neighbor_rank),
    CONSTRAINT fk_nn_revision_memberships_owner
        FOREIGN KEY (nn_revision_id, owner_device_id)
        REFERENCES nn_revisions(id, device_id)
        ON DELETE CASCADE,
    CONSTRAINT chk_nn_revision_memberships_rank_positive CHECK (neighbor_rank > 0),
    CONSTRAINT chk_nn_revision_memberships_distance_non_negative CHECK (distance_meters >= 0),
    CONSTRAINT chk_nn_revision_memberships_not_self CHECK (owner_device_id <> neighbor_device_id),
    CONSTRAINT chk_nn_revision_memberships_neighbor_latitude CHECK (
        neighbor_latitude >= -90 AND neighbor_latitude <= 90
    ),
    CONSTRAINT chk_nn_revision_memberships_neighbor_longitude CHECK (
        neighbor_longitude >= -180 AND neighbor_longitude <= 180
    )
);

CREATE TABLE nn_distribution_events (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    nn_revision_id bigint NOT NULL,
    attempt_no integer NOT NULL DEFAULT 1,
    status nn_distribution_status NOT NULL DEFAULT 'sent',
    sent_at timestamptz NOT NULL DEFAULT NOW(),
    acknowledged_at timestamptz,
    payload_metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT fk_nn_distribution_events_revision
        FOREIGN KEY (nn_revision_id, device_id)
        REFERENCES nn_revisions(id, device_id)
        ON DELETE CASCADE,
    CONSTRAINT chk_nn_distribution_events_attempt_positive CHECK (attempt_no > 0),
    CONSTRAINT chk_nn_distribution_events_ack_timestamp CHECK (
        acknowledged_at IS NULL OR acknowledged_at >= sent_at
    ),
    CONSTRAINT chk_nn_distribution_events_ack_required CHECK (
        status <> 'acknowledged' OR acknowledged_at IS NOT NULL
    ),
    CONSTRAINT chk_nn_distribution_events_metadata_object CHECK (
        jsonb_typeof(payload_metadata) = 'object'
    )
);

CREATE TABLE time_sync_events (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    target_time timestamptz NOT NULL,
    status time_sync_status NOT NULL DEFAULT 'sent',
    sent_at timestamptz NOT NULL DEFAULT NOW(),
    acknowledged_at timestamptz,
    result_message text,
    payload_metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT chk_time_sync_events_ack_timestamp CHECK (
        acknowledged_at IS NULL OR acknowledged_at >= sent_at
    ),
    CONSTRAINT chk_time_sync_events_ack_required CHECK (
        status <> 'acknowledged' OR acknowledged_at IS NOT NULL
    ),
    CONSTRAINT chk_time_sync_events_metadata_object CHECK (
        jsonb_typeof(payload_metadata) = 'object'
    )
);

CREATE TABLE device_config_deployments (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    config_revision_id bigint NOT NULL REFERENCES config_revisions(id) ON DELETE RESTRICT,
    attempt_no integer NOT NULL DEFAULT 1,
    status config_deployment_status NOT NULL DEFAULT 'sent',
    sent_at timestamptz NOT NULL DEFAULT NOW(),
    acknowledged_at timestamptz,
    applied_at timestamptz,
    payload_metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    updated_at timestamptz NOT NULL DEFAULT NOW(),
    CONSTRAINT uq_device_config_deployments_attempt UNIQUE (device_id, config_revision_id, attempt_no),
    CONSTRAINT chk_device_config_deployments_attempt_positive CHECK (attempt_no > 0),
    CONSTRAINT chk_device_config_deployments_ack_timestamp CHECK (
        acknowledged_at IS NULL OR acknowledged_at >= sent_at
    ),
    CONSTRAINT chk_device_config_deployments_applied_timestamp CHECK (
        applied_at IS NULL OR applied_at >= sent_at
    ),
    CONSTRAINT chk_device_config_deployments_ack_required CHECK (
        status <> 'acknowledged' OR acknowledged_at IS NOT NULL
    ),
    CONSTRAINT chk_device_config_deployments_metadata_object CHECK (
        jsonb_typeof(payload_metadata) = 'object'
    )
);

CREATE TABLE gateway_downlinks (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    gateway_row_id bigint NOT NULL REFERENCES gateways(id) ON DELETE CASCADE,
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    downlink_id bigint NOT NULL DEFAULT nextval('gateway_downlink_downlink_id_seq'::regclass),
    target_node_id integer NOT NULL,
    command_code smallint NOT NULL,
    backhaul_version smallint NOT NULL DEFAULT 1,
    node_packet_version smallint NOT NULL DEFAULT 1,
    status gateway_downlink_status NOT NULL DEFAULT 'pending',
    queued_at timestamptz NOT NULL DEFAULT NOW(),
    dispatched_at timestamptz,
    completed_at timestamptz,
    last_attempt_at timestamptz,
    next_attempt_at timestamptz NOT NULL DEFAULT NOW(),
    attempt_count integer NOT NULL DEFAULT 0,
    request_payload bytea NOT NULL,
    inner_payload bytea NOT NULL,
    result_status smallint,
    result_payload bytea,
    last_error text,
    metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    nn_distribution_event_id bigint UNIQUE REFERENCES nn_distribution_events(id) ON DELETE CASCADE,
    time_sync_event_id bigint UNIQUE REFERENCES time_sync_events(id) ON DELETE CASCADE,
    device_config_deployment_id bigint UNIQUE REFERENCES device_config_deployments(id) ON DELETE CASCADE,
    CONSTRAINT uq_gateway_downlinks_gateway_downlink UNIQUE (gateway_row_id, downlink_id),
    CONSTRAINT chk_gateway_downlinks_downlink_id_positive CHECK (downlink_id > 0),
    CONSTRAINT chk_gateway_downlinks_target_node_uint16 CHECK (
        target_node_id >= 0 AND target_node_id <= 65535
    ),
    CONSTRAINT chk_gateway_downlinks_command_code_supported CHECK (
        command_code IN (4, 5, 6)
    ),
    CONSTRAINT chk_gateway_downlinks_backhaul_version_uint8 CHECK (
        backhaul_version >= 0 AND backhaul_version <= 255
    ),
    CONSTRAINT chk_gateway_downlinks_node_packet_version_uint8 CHECK (
        node_packet_version >= 0 AND node_packet_version <= 255
    ),
    CONSTRAINT chk_gateway_downlinks_attempt_count_non_negative CHECK (
        attempt_count >= 0
    ),
    CONSTRAINT chk_gateway_downlinks_request_payload_non_empty CHECK (
        octet_length(request_payload) >= 20
    ),
    CONSTRAINT chk_gateway_downlinks_inner_payload_non_empty CHECK (
        octet_length(inner_payload) > 0
    ),
    CONSTRAINT chk_gateway_downlinks_result_payload_length CHECK (
        result_payload IS NULL OR octet_length(result_payload) = 19
    ),
    CONSTRAINT chk_gateway_downlinks_completed_after_queued CHECK (
        completed_at IS NULL OR completed_at >= queued_at
    ),
    CONSTRAINT chk_gateway_downlinks_dispatched_after_queued CHECK (
        dispatched_at IS NULL OR dispatched_at >= queued_at
    ),
    CONSTRAINT chk_gateway_downlinks_last_attempt_after_queued CHECK (
        last_attempt_at IS NULL OR last_attempt_at >= queued_at
    ),
    CONSTRAINT chk_gateway_downlinks_terminal_completion_required CHECK (
        status IN ('pending', 'dispatched', 'transport_failed')
        OR completed_at IS NOT NULL
    ),
    CONSTRAINT chk_gateway_downlinks_terminal_result_required CHECK (
        status IN ('pending', 'dispatched', 'transport_failed')
        OR result_status IS NOT NULL
    ),
    CONSTRAINT chk_gateway_downlinks_metadata_object CHECK (
        jsonb_typeof(metadata) = 'object'
    ),
    CONSTRAINT chk_gateway_downlinks_one_domain_reference CHECK (
        ((nn_distribution_event_id IS NOT NULL)::integer
        + (time_sync_event_id IS NOT NULL)::integer
        + (device_config_deployment_id IS NOT NULL)::integer) = 1
    )
);

ALTER SEQUENCE gateway_downlink_downlink_id_seq
    OWNED BY gateway_downlinks.downlink_id;

CREATE TABLE notification_recipients (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    display_name text,
    email_address text NOT NULL,
    is_enabled boolean NOT NULL DEFAULT true,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    updated_at timestamptz NOT NULL DEFAULT NOW(),
    CONSTRAINT chk_notification_recipients_email_shape CHECK (
        position('@' IN email_address) > 1
    )
);

CREATE TABLE notification_preferences (
    recipient_id bigint NOT NULL REFERENCES notification_recipients(id) ON DELETE CASCADE,
    event_type notification_event_type NOT NULL,
    is_enabled boolean NOT NULL DEFAULT true,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    updated_at timestamptz NOT NULL DEFAULT NOW(),
    PRIMARY KEY (recipient_id, event_type)
);

CREATE TABLE notification_deliveries (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    recipient_id bigint NOT NULL REFERENCES notification_recipients(id) ON DELETE CASCADE,
    event_type notification_event_type NOT NULL,
    alert_id bigint REFERENCES alerts(id) ON DELETE SET NULL,
    device_id bigint REFERENCES devices(id) ON DELETE SET NULL,
    event_occurred_at timestamptz NOT NULL,
    subject text NOT NULL,
    status notification_delivery_status NOT NULL DEFAULT 'queued',
    queued_at timestamptz NOT NULL DEFAULT NOW(),
    attempted_at timestamptz,
    delivered_at timestamptz,
    provider_message_id text,
    failure_reason text,
    payload_snapshot jsonb NOT NULL DEFAULT '{}'::jsonb,
    CONSTRAINT chk_notification_deliveries_subject_not_blank CHECK (
        length(btrim(subject)) > 0
    ),
    CONSTRAINT chk_notification_deliveries_attempt_timestamp CHECK (
        attempted_at IS NULL OR attempted_at >= queued_at
    ),
    CONSTRAINT chk_notification_deliveries_delivery_timestamp CHECK (
        delivered_at IS NULL OR delivered_at >= queued_at
    ),
    CONSTRAINT chk_notification_deliveries_sent_requires_delivered_at CHECK (
        status <> 'sent' OR delivered_at IS NOT NULL
    ),
    CONSTRAINT chk_notification_deliveries_payload_object CHECK (
        jsonb_typeof(payload_snapshot) = 'object'
    )
);

CREATE TABLE sensor_reading_hourly_aggregates (
    device_id bigint NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    bucket_start timestamptz NOT NULL,
    bucket_end timestamptz NOT NULL,
    sample_count integer NOT NULL,
    min_temperature_c numeric(6,2),
    avg_temperature_c numeric(7,3),
    max_temperature_c numeric(6,2),
    min_humidity_pct numeric(5,2),
    avg_humidity_pct numeric(6,3),
    max_humidity_pct numeric(5,2),
    min_voc_iaq integer,
    avg_voc_iaq numeric(10,2),
    max_voc_iaq integer,
    min_pm25_ug_m3 numeric(8,1),
    avg_pm25_ug_m3 numeric(9,2),
    max_pm25_ug_m3 numeric(8,1),
    max_risk_level smallint,
    first_reported_at timestamptz NOT NULL,
    last_reported_at timestamptz NOT NULL,
    created_at timestamptz NOT NULL DEFAULT NOW(),
    PRIMARY KEY (device_id, bucket_start),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_bucket_start_hour CHECK (
        bucket_start = date_trunc('hour', bucket_start)
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_bucket_end CHECK (
        bucket_end = bucket_start + interval '1 hour'
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_sample_count CHECK (sample_count > 0),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_humidity_min CHECK (
        min_humidity_pct IS NULL OR (min_humidity_pct >= 0 AND min_humidity_pct <= 100)
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_humidity_avg CHECK (
        avg_humidity_pct IS NULL OR (avg_humidity_pct >= 0 AND avg_humidity_pct <= 100)
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_humidity_max CHECK (
        max_humidity_pct IS NULL OR (max_humidity_pct >= 0 AND max_humidity_pct <= 100)
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_voc_min CHECK (
        min_voc_iaq IS NULL OR min_voc_iaq >= 0
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_voc_avg CHECK (
        avg_voc_iaq IS NULL OR avg_voc_iaq >= 0
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_voc_max CHECK (
        max_voc_iaq IS NULL OR max_voc_iaq >= 0
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_pm25_min CHECK (
        min_pm25_ug_m3 IS NULL OR min_pm25_ug_m3 >= 0
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_pm25_avg CHECK (
        avg_pm25_ug_m3 IS NULL OR avg_pm25_ug_m3 >= 0
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_pm25_max CHECK (
        max_pm25_ug_m3 IS NULL OR max_pm25_ug_m3 >= 0
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_risk CHECK (
        max_risk_level IS NULL OR max_risk_level BETWEEN 1 AND 5
    ),
    CONSTRAINT chk_sensor_reading_hourly_aggregates_reported_window CHECK (
        last_reported_at >= first_reported_at
    )
);

ALTER TABLE devices
    ADD COLUMN current_nn_revision_id bigint;

ALTER TABLE devices
    ADD CONSTRAINT fk_devices_current_nn_revision
    FOREIGN KEY (current_nn_revision_id)
    REFERENCES nn_revisions(id)
    ON DELETE SET NULL;

CREATE UNIQUE INDEX uq_device_ipv6_history_current_row
    ON device_ipv6_history (device_id)
    WHERE valid_to IS NULL;

CREATE UNIQUE INDEX uq_device_ipv6_history_current_address
    ON device_ipv6_history (ipv6_address)
    WHERE valid_to IS NULL;

CREATE UNIQUE INDEX uq_nn_revisions_current_per_device
    ON nn_revisions (device_id)
    WHERE active_to IS NULL;

CREATE UNIQUE INDEX uq_notification_recipients_email_lower
    ON notification_recipients (lower(email_address));

CREATE INDEX idx_gateways_last_registered_at
    ON gateways (last_registered_at DESC);

CREATE INDEX idx_gateway_registrations_gateway_reported_at
    ON gateway_registrations (gateway_row_id, reported_at DESC);

CREATE INDEX idx_gateway_registrations_ingested_at
    ON gateway_registrations (ingested_at DESC);

CREATE INDEX idx_gateway_uplinks_gateway_received_at
    ON gateway_uplinks (gateway_row_id, received_at DESC, uplink_id DESC);

CREATE INDEX idx_gateway_uplinks_storage_status_received_at
    ON gateway_uplinks (storage_status, first_csp_received_at DESC);

CREATE INDEX idx_gateway_uplinks_observed_src_ipv6_received_at
    ON gateway_uplinks (observed_src_ipv6, received_at DESC);

CREATE INDEX idx_gateway_uplink_receipts_status_responded_at
    ON gateway_uplink_receipts (status, responded_at DESC);

CREATE INDEX idx_gateway_uplink_dead_letters_dead_lettered_at
    ON gateway_uplink_dead_letters (dead_lettered_at DESC);

CREATE INDEX idx_gateway_downlinks_status_next_attempt
    ON gateway_downlinks (status, next_attempt_at, queued_at);

CREATE INDEX idx_gateway_downlinks_gateway_queued_at
    ON gateway_downlinks (gateway_row_id, queued_at DESC);

CREATE INDEX idx_devices_last_seen_at
    ON devices (last_seen_at DESC);

CREATE INDEX idx_devices_latest_reported_at
    ON devices (latest_reported_at DESC);

CREATE INDEX idx_device_registrations_device_observed_at
    ON device_registrations (device_id, observed_at DESC);

CREATE INDEX idx_device_registrations_gateway_uplink
    ON device_registrations (gateway_uplink_id)
    WHERE gateway_uplink_id IS NOT NULL;

CREATE INDEX idx_device_registrations_ipv6_observed_at
    ON device_registrations (observed_ipv6, observed_at DESC);

CREATE INDEX idx_device_ipv6_history_device_valid_from
    ON device_ipv6_history (device_id, valid_from DESC);

CREATE INDEX idx_device_parent_observations_device_observed_at
    ON device_parent_observations (device_id, observed_at DESC);

CREATE INDEX idx_sensor_readings_device_reported_at
    ON sensor_readings (device_id, reported_at DESC);

CREATE INDEX idx_sensor_readings_gateway_uplink
    ON sensor_readings (gateway_uplink_id)
    WHERE gateway_uplink_id IS NOT NULL;

CREATE INDEX idx_sensor_readings_device_ingested_at
    ON sensor_readings (device_id, ingested_at DESC);

CREATE INDEX idx_sensor_readings_critical_alerts
    ON sensor_readings (reported_at DESC)
    WHERE source_type = 'critical_alert';

CREATE INDEX idx_alerts_active_detected_at
    ON alerts (detected_at DESC)
    WHERE status <> 'cleared';

CREATE INDEX idx_alerts_device_detected_at
    ON alerts (device_id, detected_at DESC);

CREATE INDEX idx_alert_events_alert_event_at
    ON alert_events (alert_id, event_at DESC);

CREATE INDEX idx_nn_revisions_device_active_from
    ON nn_revisions (device_id, active_from DESC);

CREATE INDEX idx_nn_revision_memberships_revision_rank
    ON nn_revision_memberships (nn_revision_id, neighbor_rank);

CREATE INDEX idx_nn_revision_memberships_neighbor_device
    ON nn_revision_memberships (neighbor_device_id);

CREATE INDEX idx_nn_distribution_events_device_sent_at
    ON nn_distribution_events (device_id, sent_at DESC);

CREATE INDEX idx_nn_distribution_events_revision_sent_at
    ON nn_distribution_events (nn_revision_id, sent_at DESC);

CREATE INDEX idx_time_sync_events_device_sent_at
    ON time_sync_events (device_id, sent_at DESC);

CREATE INDEX idx_time_sync_events_status_sent_at
    ON time_sync_events (status, sent_at DESC);

CREATE INDEX idx_config_revisions_activated_at
    ON config_revisions (activated_at DESC NULLS LAST, config_id DESC);

CREATE INDEX idx_device_config_deployments_device_sent_at
    ON device_config_deployments (device_id, sent_at DESC);

CREATE INDEX idx_device_config_deployments_config_status
    ON device_config_deployments (config_revision_id, status, sent_at DESC);

CREATE INDEX idx_notification_deliveries_recipient_queued_at
    ON notification_deliveries (recipient_id, queued_at DESC);

CREATE INDEX idx_notification_deliveries_event_queued_at
    ON notification_deliveries (event_type, queued_at DESC);

CREATE INDEX idx_notification_deliveries_status_queued_at
    ON notification_deliveries (status, queued_at DESC);

CREATE INDEX idx_sensor_reading_hourly_aggregates_bucket_start
    ON sensor_reading_hourly_aggregates (bucket_start DESC);

CREATE OR REPLACE FUNCTION enforce_config_revision_config_id_monotonicity()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
    current_max_config_id bigint;
    sequence_last_value bigint;
    sequence_is_called boolean;
    last_allocated_config_id bigint;
BEGIN
    SELECT COALESCE(MAX(config_id), 0)
    INTO current_max_config_id
    FROM config_revisions;

    IF NEW.config_id <= current_max_config_id THEN
        RAISE EXCEPTION
            'config_id % must be greater than the current max config_id %',
            NEW.config_id,
            current_max_config_id
            USING ERRCODE = '23514';
    END IF;

    SELECT last_value, is_called
    INTO sequence_last_value, sequence_is_called
    FROM config_revisions_config_id_seq;

    last_allocated_config_id := CASE
        WHEN sequence_is_called THEN sequence_last_value
        ELSE sequence_last_value - 1
    END;

    IF NEW.config_id > last_allocated_config_id THEN
        PERFORM setval('config_revisions_config_id_seq', NEW.config_id, true);
    END IF;

    RETURN NEW;
END;
$$;

CREATE OR REPLACE FUNCTION prevent_config_revision_config_id_update()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
    IF NEW.config_id <> OLD.config_id THEN
        RAISE EXCEPTION
            'config_id is immutable once a config revision has been created'
            USING ERRCODE = '23514';
    END IF;

    RETURN NEW;
END;
$$;

CREATE OR REPLACE FUNCTION enforce_nn_revision_membership_radius()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
    parent_radius_meters integer;
BEGIN
    SELECT radius_meters
    INTO parent_radius_meters
    FROM nn_revisions
    WHERE id = NEW.nn_revision_id;

    IF parent_radius_meters IS NULL THEN
        RAISE EXCEPTION
            'nn_revision % does not exist for membership validation',
            NEW.nn_revision_id
            USING ERRCODE = '23503';
    END IF;

    IF NEW.distance_meters > parent_radius_meters THEN
        RAISE EXCEPTION
            'distance_meters % exceeds nn revision radius_meters % for nn_revision_id %',
            NEW.distance_meters,
            parent_radius_meters,
            NEW.nn_revision_id
            USING ERRCODE = '23514';
    END IF;

    RETURN NEW;
END;
$$;

CREATE OR REPLACE FUNCTION prevent_nn_revision_radius_shrink_below_memberships()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
    IF NEW.radius_meters < OLD.radius_meters
       AND EXISTS (
           SELECT 1
           FROM nn_revision_memberships
           WHERE nn_revision_id = NEW.id
             AND distance_meters > NEW.radius_meters
       ) THEN
        RAISE EXCEPTION
            'radius_meters % is below one or more existing membership distances for nn_revision_id %',
            NEW.radius_meters,
            NEW.id
            USING ERRCODE = '23514';
    END IF;

    RETURN NEW;
END;
$$;

CREATE TRIGGER trg_enforce_config_revision_config_id_monotonicity
BEFORE INSERT ON config_revisions
FOR EACH ROW
WHEN (NEW.config_id IS NOT NULL)
EXECUTE FUNCTION enforce_config_revision_config_id_monotonicity();

CREATE TRIGGER trg_set_updated_at_config_revisions
BEFORE UPDATE ON config_revisions
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

CREATE TRIGGER trg_set_updated_at_gateways
BEFORE UPDATE ON gateways
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

CREATE TRIGGER trg_prevent_config_revision_config_id_update
BEFORE UPDATE OF config_id ON config_revisions
FOR EACH ROW
EXECUTE FUNCTION prevent_config_revision_config_id_update();

CREATE TRIGGER trg_set_updated_at_devices
BEFORE UPDATE ON devices
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

CREATE TRIGGER trg_set_updated_at_alerts
BEFORE UPDATE ON alerts
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

CREATE TRIGGER trg_set_updated_at_nn_revisions
BEFORE UPDATE ON nn_revisions
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

CREATE TRIGGER trg_prevent_nn_revision_radius_shrink_below_memberships
BEFORE UPDATE OF radius_meters ON nn_revisions
FOR EACH ROW
EXECUTE FUNCTION prevent_nn_revision_radius_shrink_below_memberships();

CREATE TRIGGER trg_enforce_nn_revision_membership_radius
BEFORE INSERT OR UPDATE ON nn_revision_memberships
FOR EACH ROW
EXECUTE FUNCTION enforce_nn_revision_membership_radius();

CREATE TRIGGER trg_set_updated_at_device_config_deployments
BEFORE UPDATE ON device_config_deployments
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

CREATE TRIGGER trg_set_updated_at_notification_recipients
BEFORE UPDATE ON notification_recipients
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

CREATE TRIGGER trg_set_updated_at_notification_preferences
BEFORE UPDATE ON notification_preferences
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

COMMENT ON TABLE devices IS
'Current per-device snapshot optimized for dashboard and map queries. Stores current network identity, location, registration window, and cached latest telemetry.';

COMMENT ON TABLE device_registrations IS
'Append-only audit log of device registration and rejoin events, including the observed IPv6 address and registration-time device metadata.';

COMMENT ON TABLE device_ipv6_history IS
'Tracks validity windows for each IPv6 address observed for a device. The row with NULL valid_to is the current address assignment.';

COMMENT ON TABLE device_parent_observations IS
'Append-only history of preferred-parent observations sourced from registration packets and 0x08 parent-update uplinks.';

COMMENT ON TABLE sensor_readings IS
'Append-only telemetry history for sensor reports and sensor alerts. Raw rows should remain available for at least 24 hours even if longer-term aggregates are also maintained.';

COMMENT ON TABLE alerts IS
'Alert records with lifecycle state, timestamps, and metric snapshots captured at alert time.';

COMMENT ON TABLE alert_events IS
'Append-only lifecycle history for alerts, preserving acknowledgement and clearing audit events.';

COMMENT ON TABLE nn_revisions IS
'Versioned nearest-neighbor sets per device. The row with NULL active_to is the currently active neighbor set for that device.';

COMMENT ON TABLE nn_revision_memberships IS
'Neighbors belonging to a nearest-neighbor revision, including rank, distance, and coordinate snapshot at generation time.';

COMMENT ON TABLE nn_distribution_events IS
'Audit trail of nearest-neighbor table transmissions to devices.';

COMMENT ON TABLE config_revisions IS
'Versioned wildfire risk threshold definitions keyed by the monotonic config_id used in packet 0x06.';

COMMENT ON TABLE gateways IS
'Current per-gateway snapshot keyed by the stable logical gateway_id used by the binary backhaul protocol. Rows may exist before a 0x81 registration arrives so raw 0x82 uplinks can be stored durably.';

COMMENT ON TABLE gateway_registrations IS
'Append-only audit log of gateway registration messages received over the backhaul API, including the raw 0x81 payload.';

COMMENT ON TABLE gateway_uplinks IS
'Durable raw inbox for CSP-bound 0x82 uplink envelopes. One row represents the idempotency key (gateway_id, uplink_id) plus raw bytes needed for replay.';

COMMENT ON TABLE gateway_uplink_receipts IS
'Terminal 0x83 receipt state for stored gateway uplinks. Duplicate 0x82 deliveries should reuse the same terminal receipt.';

COMMENT ON TABLE gateway_uplink_dead_letters IS
'Operator-visible dead-letter records for permanently rejected uplinks, preserving rejection reasons separately from the raw inbox.';

COMMENT ON TABLE gateway_downlinks IS
'Durable BR-facing outbox for CSP-originated 0x84 downlink requests and terminal 0x85 results.';

COMMENT ON TABLE device_config_deployments IS
'Per-device audit log of configuration pushes and acknowledgements for config revisions.';

COMMENT ON TABLE time_sync_events IS
'Per-device log of daily time synchronization attempts, sent time values, and acknowledgement status.';

COMMENT ON TABLE notification_recipients IS
'Recipients eligible to receive configured alert notifications by email or SMS.';

COMMENT ON TABLE notification_preferences IS
'Per-recipient enable/disable settings for each notification event type.';

COMMENT ON TABLE notification_deliveries IS
'Delivery audit for attempted notification emails, including timestamps, status, and payload snapshot.';

COMMENT ON TABLE sensor_reading_hourly_aggregates IS
'Optional long-term hourly rollups derived from raw sensor_readings for reporting beyond the raw retention window.';

COMMENT ON COLUMN devices.current_ipv6 IS
'Authoritative current network address for the device. This value may change when a node rejoins.';

COMMENT ON COLUMN devices.current_parent_ipv6 IS
'Latest preferred RPL parent IPv6 observed by the CSP from registration or parent-update uplinks. This is routing state, not CSP-managed nearest-neighbor membership.';

COMMENT ON COLUMN devices.last_observed_gateway_row_id IS
'Most recent gateway row that delivered a valid projected uplink for this device. Used as the CSP downlink routing basis until a fresher observation arrives.';

COMMENT ON COLUMN devices.last_observed_gateway_at IS
'Observation timestamp corresponding to last_observed_gateway_row_id.';

COMMENT ON COLUMN gateway_uplinks.uplink_id IS
'Unsigned 64-bit gateway-assigned idempotency key stored as numeric(20,0) because PostgreSQL bigint cannot represent the full uint64 range.';

COMMENT ON COLUMN gateway_uplinks.raw_envelope IS
'Original binary 0x82 message body as received by the CSP, retained for replay and audit.';

COMMENT ON COLUMN sensor_readings.raw_payload_metadata IS
'Supplemental packet metadata such as original epoch seconds, packet counters, or gateway ingestion details.';

COMMENT ON COLUMN alerts.details IS
'Machine-readable alert details that do not belong in fixed relational columns.';

COMMIT;
