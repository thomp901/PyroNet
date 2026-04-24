BEGIN;

ALTER TABLE gateway_uplinks
    DROP CONSTRAINT IF EXISTS chk_gateway_uplinks_ipv6_global_unicast,
    ADD CONSTRAINT chk_gateway_uplinks_ipv6_global_unicast CHECK (
        observed_src_ipv6 IS NULL OR (
            family(observed_src_ipv6) = 6
            AND NOT (observed_src_ipv6 <<= inet '::/128')
            AND NOT (observed_src_ipv6 <<= inet '::1/128')
            AND NOT (observed_src_ipv6 <<= inet 'ff00::/8')
        )
    );

ALTER TABLE devices
    DROP CONSTRAINT IF EXISTS chk_devices_current_ipv6_global_unicast,
    ADD CONSTRAINT chk_devices_current_ipv6_global_unicast CHECK (
        current_ipv6 IS NULL OR (
            family(current_ipv6) = 6
            AND NOT (current_ipv6 <<= inet '::/128')
            AND NOT (current_ipv6 <<= inet '::1/128')
            AND NOT (current_ipv6 <<= inet 'ff00::/8')
        )
    ),
    DROP CONSTRAINT IF EXISTS chk_devices_current_parent_ipv6_global_unicast,
    ADD CONSTRAINT chk_devices_current_parent_ipv6_global_unicast CHECK (
        current_parent_ipv6 IS NULL OR (
            family(current_parent_ipv6) = 6
            AND NOT (current_parent_ipv6 <<= inet '::/128')
            AND NOT (current_parent_ipv6 <<= inet '::1/128')
            AND NOT (current_parent_ipv6 <<= inet 'ff00::/8')
        )
    );

ALTER TABLE device_registrations
    DROP CONSTRAINT IF EXISTS chk_device_registrations_ipv6_global_unicast,
    ADD CONSTRAINT chk_device_registrations_ipv6_global_unicast CHECK (
        family(observed_ipv6) = 6
        AND NOT (observed_ipv6 <<= inet '::/128')
        AND NOT (observed_ipv6 <<= inet '::1/128')
        AND NOT (observed_ipv6 <<= inet 'ff00::/8')
    );

ALTER TABLE device_ipv6_history
    DROP CONSTRAINT IF EXISTS chk_device_ipv6_history_ipv6_global_unicast,
    ADD CONSTRAINT chk_device_ipv6_history_ipv6_global_unicast CHECK (
        family(ipv6_address) = 6
        AND NOT (ipv6_address <<= inet '::/128')
        AND NOT (ipv6_address <<= inet '::1/128')
        AND NOT (ipv6_address <<= inet 'ff00::/8')
    );

ALTER TABLE device_parent_observations
    DROP CONSTRAINT IF EXISTS chk_device_parent_observations_ipv6_global_unicast,
    ADD CONSTRAINT chk_device_parent_observations_ipv6_global_unicast CHECK (
        observed_parent_ipv6 IS NULL OR (
            family(observed_parent_ipv6) = 6
            AND NOT (observed_parent_ipv6 <<= inet '::/128')
            AND NOT (observed_parent_ipv6 <<= inet '::1/128')
            AND NOT (observed_parent_ipv6 <<= inet 'ff00::/8')
        )
    );

COMMIT;
