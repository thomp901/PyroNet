from __future__ import annotations


MIGRATIONS = {
    1: """
    CREATE TABLE IF NOT EXISTS schema_meta (
        version INTEGER NOT NULL
    );

    CREATE TABLE IF NOT EXISTS gateways (
        gateway_id INTEGER PRIMARY KEY,
        latitude DOUBLE PRECISION,
        longitude DOUBLE PRECISION,
        sw_version INTEGER,
        last_registered_at BIGINT,
        created_at BIGINT NOT NULL,
        updated_at BIGINT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS gateway_registrations (
        id BIGSERIAL PRIMARY KEY,
        gateway_id INTEGER NOT NULL REFERENCES gateways(gateway_id),
        timestamp BIGINT NOT NULL,
        raw_payload BYTEA NOT NULL,
        created_at BIGINT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS uplink_receipts (
        gateway_id INTEGER NOT NULL,
        uplink_id BIGINT NOT NULL,
        terminal_status INTEGER NOT NULL,
        stored_receipt BYTEA NOT NULL,
        detail TEXT,
        created_at BIGINT NOT NULL,
        PRIMARY KEY (gateway_id, uplink_id)
    );

    CREATE TABLE IF NOT EXISTS uplink_envelopes (
        gateway_id INTEGER NOT NULL,
        uplink_id BIGINT NOT NULL,
        version INTEGER NOT NULL,
        received_at BIGINT NOT NULL,
        observed_src_ipv6 TEXT NOT NULL,
        payload_type INTEGER NOT NULL,
        node_id INTEGER NOT NULL,
        raw_envelope BYTEA NOT NULL,
        payload_bytes BYTEA NOT NULL,
        created_at BIGINT NOT NULL,
        PRIMARY KEY (gateway_id, uplink_id)
    );

    CREATE TABLE IF NOT EXISTS nodes (
        node_id INTEGER PRIMARY KEY,
        current_ipv6 TEXT,
        latitude DOUBLE PRECISION,
        longitude DOUBLE PRECISION,
        firmware_version INTEGER,
        latest_battery INTEGER,
        latest_risk_level INTEGER,
        latest_temperature INTEGER,
        latest_humidity INTEGER,
        latest_voc INTEGER,
        latest_pm25 INTEGER,
        last_seen BIGINT,
        last_gateway_id INTEGER,
        latest_parent_ipv6 TEXT,
        created_at BIGINT NOT NULL,
        updated_at BIGINT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS sensor_readings (
        id BIGSERIAL PRIMARY KEY,
        gateway_id INTEGER NOT NULL,
        uplink_id BIGINT NOT NULL,
        packet_type INTEGER NOT NULL,
        node_id INTEGER NOT NULL,
        node_event_time BIGINT NOT NULL,
        gateway_received_at BIGINT NOT NULL,
        observed_src_ipv6 TEXT NOT NULL,
        temperature INTEGER NOT NULL,
        humidity INTEGER NOT NULL,
        voc INTEGER NOT NULL,
        pm25 INTEGER NOT NULL,
        risk_level INTEGER NOT NULL,
        battery_pct INTEGER NOT NULL,
        raw_payload BYTEA NOT NULL,
        created_at BIGINT NOT NULL,
        UNIQUE (gateway_id, uplink_id)
    );

    CREATE TABLE IF NOT EXISTS topology_events (
        id BIGSERIAL PRIMARY KEY,
        gateway_id INTEGER NOT NULL,
        uplink_id BIGINT NOT NULL,
        packet_type INTEGER NOT NULL,
        node_id INTEGER NOT NULL,
        parent_ipv6 TEXT,
        node_event_time BIGINT,
        gateway_received_at BIGINT NOT NULL,
        raw_payload BYTEA NOT NULL,
        created_at BIGINT NOT NULL,
        UNIQUE (gateway_id, uplink_id)
    );

    CREATE TABLE IF NOT EXISTS alerts (
        id BIGSERIAL PRIMARY KEY,
        gateway_id INTEGER NOT NULL,
        uplink_id BIGINT NOT NULL,
        node_id INTEGER NOT NULL,
        node_event_time BIGINT NOT NULL,
        gateway_received_at BIGINT NOT NULL,
        risk_level INTEGER NOT NULL,
        battery_pct INTEGER NOT NULL,
        raw_payload BYTEA NOT NULL,
        created_at BIGINT NOT NULL,
        UNIQUE (gateway_id, uplink_id)
    );
    """,
    2: """
    ALTER TABLE uplink_envelopes ALTER COLUMN node_id DROP NOT NULL;

    CREATE TABLE IF NOT EXISTS malformed_uplink_rejects (
        gateway_id INTEGER NOT NULL,
        uplink_id BIGINT NOT NULL,
        version INTEGER,
        raw_request_body BYTEA NOT NULL,
        reject_reason TEXT NOT NULL,
        created_at BIGINT NOT NULL,
        PRIMARY KEY (gateway_id, uplink_id)
    );
    """,
    3: """
    CREATE TABLE IF NOT EXISTS control_sequences (
        name TEXT PRIMARY KEY,
        next_value BIGINT NOT NULL
    );

    INSERT INTO control_sequences(name, next_value)
    VALUES ('config_id', 1)
    ON CONFLICT (name) DO NOTHING;

    ALTER TABLE nodes ADD COLUMN connectivity_state TEXT;

    CREATE TABLE IF NOT EXISTS risk_configs (
        config_id BIGINT PRIMARY KEY,
        l2_temp_thresh INTEGER NOT NULL,
        l2_humidity_thresh INTEGER NOT NULL,
        l2_voc_thresh INTEGER NOT NULL,
        l3_temp_thresh INTEGER NOT NULL,
        l3_humidity_thresh INTEGER NOT NULL,
        l3_voc_thresh INTEGER NOT NULL,
        l4_voc_thresh INTEGER NOT NULL,
        l5_voc_thresh INTEGER NOT NULL,
        l5_pm25_thresh INTEGER NOT NULL,
        created_at BIGINT NOT NULL,
        created_by TEXT,
        source TEXT,
        comment TEXT
    );

    CREATE TABLE IF NOT EXISTS risk_config_targets (
        config_id BIGINT NOT NULL REFERENCES risk_configs(config_id),
        node_id INTEGER NOT NULL,
        desired_state TEXT NOT NULL,
        latest_attempt_status TEXT,
        latest_attempt_time BIGINT,
        latest_result_payload TEXT,
        PRIMARY KEY (config_id, node_id)
    );

    CREATE TABLE IF NOT EXISTS neighbor_sets (
        node_id INTEGER PRIMARY KEY,
        computed_at BIGINT NOT NULL,
        radius_meters DOUBLE PRECISION NOT NULL,
        max_neighbors INTEGER,
        neighbor_node_ids TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS neighbor_set_members (
        node_id INTEGER NOT NULL,
        neighbor_node_id INTEGER NOT NULL,
        computed_at BIGINT NOT NULL,
        PRIMARY KEY (node_id, neighbor_node_id)
    );

    CREATE TABLE IF NOT EXISTS control_attempts (
        id BIGSERIAL PRIMARY KEY,
        request_type TEXT NOT NULL,
        target_node_id INTEGER NOT NULL,
        request_body TEXT NOT NULL,
        gateway_url TEXT NOT NULL,
        gateway_id INTEGER,
        scheduler_run_id BIGINT,
        response_status INTEGER,
        response_body TEXT,
        delivery_result TEXT NOT NULL,
        created_at BIGINT NOT NULL,
        completed_at BIGINT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS scheduler_runs (
        id BIGSERIAL PRIMARY KEY,
        job_type TEXT NOT NULL,
        started_at BIGINT NOT NULL,
        completed_at BIGINT,
        status TEXT NOT NULL,
        details TEXT
    );

    CREATE TABLE IF NOT EXISTS node_connectivity_events (
        id BIGSERIAL PRIMARY KEY,
        node_id INTEGER NOT NULL,
        event_type TEXT NOT NULL,
        created_at BIGINT NOT NULL,
        detail TEXT
    );
    """,
}

LATEST_SCHEMA_VERSION = max(MIGRATIONS)
