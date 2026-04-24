BEGIN;

ALTER TABLE gateways
    ADD COLUMN IF NOT EXISTS current_ipv6 inet;

ALTER TABLE gateways
    DROP CONSTRAINT IF EXISTS uq_gateways_current_ipv6,
    ADD CONSTRAINT uq_gateways_current_ipv6 UNIQUE (current_ipv6),
    DROP CONSTRAINT IF EXISTS chk_gateways_current_ipv6_global_unicast,
    ADD CONSTRAINT chk_gateways_current_ipv6_global_unicast CHECK (
        current_ipv6 IS NULL OR (
            family(current_ipv6) = 6
            AND NOT (current_ipv6 <<= inet '::/128')
            AND NOT (current_ipv6 <<= inet '::1/128')
            AND NOT (current_ipv6 <<= inet 'ff00::/8')
        )
    );

ALTER TABLE gateway_registrations
    ADD COLUMN IF NOT EXISTS wisun_ipv6 inet;

ALTER TABLE gateway_registrations
    DROP CONSTRAINT IF EXISTS chk_gateway_registrations_wisun_ipv6_global_unicast,
    ADD CONSTRAINT chk_gateway_registrations_wisun_ipv6_global_unicast CHECK (
        wisun_ipv6 IS NULL OR (
            family(wisun_ipv6) = 6
            AND NOT (wisun_ipv6 <<= inet '::/128')
            AND NOT (wisun_ipv6 <<= inet '::1/128')
            AND NOT (wisun_ipv6 <<= inet 'ff00::/8')
        )
    ),
    DROP CONSTRAINT IF EXISTS chk_gateway_registrations_raw_message_length,
    ADD CONSTRAINT chk_gateway_registrations_raw_message_length CHECK (
        octet_length(raw_message) IN (18, 34)
    );

COMMIT;
