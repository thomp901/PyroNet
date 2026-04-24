from __future__ import annotations

import importlib.metadata
import os
import tomllib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Mapping

DEFAULT_BACKHAUL_BASE_URL = "http://100.115.252.76:4000"


def pack_sw_version(value: str | int) -> int:
    if isinstance(value, int):
        return value
    if "." in value:
        major_text, minor_text = value.split(".", 1)
        major = int(major_text, 10)
        minor = int(minor_text, 10)
        if not 0 <= major <= 0xFF or not 0 <= minor <= 0xFF:
            raise ValueError("software version components must fit in 8.8 format")
        return (major << 8) | minor
    return int(value, 0)


def resolve_sw_version(override: str | int | None, *, package_name: str = "pyronet-gateway") -> int:
    if override is not None:
        return pack_sw_version(override)
    try:
        version = importlib.metadata.version(package_name)
    except importlib.metadata.PackageNotFoundError:
        return 0
    parts = version.split(".")
    major = int(parts[0], 10) if parts else 0
    minor = int(parts[1], 10) if len(parts) > 1 else 0
    return pack_sw_version(f"{major}.{minor}")


@dataclass(frozen=True)
class CoapConfig:
    bind_host: str = "::"
    port: int = 5683
    resource_path: str = "/uplink"
    duplicate_cache_ttl_seconds: float = 60.0
    duplicate_cache_max_entries: int = 1024
    downlink_port: int = 5683
    downlink_resource_path: str = "/downlink"
    ack_timeout_seconds: float = 2.0
    max_retransmit: int = 4


@dataclass(frozen=True)
class BackhaulConfig:
    base_url: str = DEFAULT_BACKHAUL_BASE_URL
    http_timeout_seconds: float = 5.0
    registration_retry_base_delay_seconds: int = 5
    registration_retry_max_delay_seconds: int = 300
    uplink_retry_base_delay_seconds: int = 5
    uplink_retry_max_delay_seconds: int = 300


@dataclass(frozen=True)
class RuntimeConfig:
    db_path: Path
    log_level: str = "INFO"
    worker_poll_interval_seconds: float = 1.0


@dataclass(frozen=True)
class HttpApiConfig:
    bind_host: str = "::"
    port: int = 8081
    max_request_body_bytes: int = 65536


@dataclass(frozen=True)
class GatewayConfig:
    gateway_id: int
    latitude: float
    longitude: float
    wisun_ipv6: str
    sw_version: int
    coap: CoapConfig
    backhaul: BackhaulConfig
    runtime: RuntimeConfig
    backhaul_version: int = 1
    node_packet_version: int = 1
    http_api: HttpApiConfig = field(default_factory=HttpApiConfig)


ENV_OVERRIDES = {
    "PYRONET_GATEWAY_ID": ("gateway", "gateway_id"),
    "PYRONET_GATEWAY_LATITUDE": ("gateway", "latitude"),
    "PYRONET_GATEWAY_LONGITUDE": ("gateway", "longitude"),
    "PYRONET_GATEWAY_WISUN_IPV6": ("gateway", "wisun_ipv6"),
    "PYRONET_SW_VERSION_OVERRIDE": ("gateway", "sw_version_override"),
    "PYRONET_COAP_BIND_HOST": ("coap", "bind_host"),
    "PYRONET_COAP_BIND_PORT": ("coap", "port"),
    "PYRONET_COAP_RESOURCE_PATH": ("coap", "resource_path"),
    "PYRONET_COAP_DUPLICATE_CACHE_TTL_SECONDS": ("coap", "duplicate_cache_ttl_seconds"),
    "PYRONET_COAP_DUPLICATE_CACHE_MAX_ENTRIES": ("coap", "duplicate_cache_max_entries"),
    "PYRONET_COAP_DOWNLINK_PORT": ("coap", "downlink_port"),
    "PYRONET_COAP_DOWNLINK_RESOURCE_PATH": ("coap", "downlink_resource_path"),
    "PYRONET_COAP_ACK_TIMEOUT_SECONDS": ("coap", "ack_timeout_seconds"),
    "PYRONET_COAP_MAX_RETRANSMIT": ("coap", "max_retransmit"),
    "PYRONET_BACKHAUL_BASE_URL": ("backhaul", "base_url"),
    "PYRONET_HTTP_TIMEOUT_SECONDS": ("backhaul", "http_timeout_seconds"),
    "PYRONET_REGISTER_RETRY_BASE_DELAY_SECONDS": ("backhaul", "registration_retry_base_delay_seconds"),
    "PYRONET_REGISTER_RETRY_MAX_DELAY_SECONDS": ("backhaul", "registration_retry_max_delay_seconds"),
    "PYRONET_UPLINK_RETRY_BASE_DELAY_SECONDS": ("backhaul", "uplink_retry_base_delay_seconds"),
    "PYRONET_UPLINK_RETRY_MAX_DELAY_SECONDS": ("backhaul", "uplink_retry_max_delay_seconds"),
    "PYRONET_DB_PATH": ("runtime", "db_path"),
    "PYRONET_WORKER_POLL_INTERVAL_SECONDS": ("runtime", "worker_poll_interval_seconds"),
    "PYRONET_LOG_LEVEL": ("runtime", "log_level"),
    "PYRONET_HTTP_API_BIND_HOST": ("http_api", "bind_host"),
    "PYRONET_HTTP_API_PORT": ("http_api", "port"),
    "PYRONET_HTTP_API_MAX_REQUEST_BODY_BYTES": ("http_api", "max_request_body_bytes"),
}


def load_config(path: str | Path, *, environ: Mapping[str, str] | None = None) -> GatewayConfig:
    config_path = Path(path)
    data = tomllib.loads(config_path.read_text())
    values = {
        "gateway": dict(data.get("gateway", {})),
        "coap": dict(data.get("coap", {})),
        "backhaul": dict(data.get("backhaul", {})),
        "runtime": dict(data.get("runtime", {})),
        "http_api": dict(data.get("http_api", {})),
    }
    env = dict(os.environ if environ is None else environ)
    for env_name, (section, key) in ENV_OVERRIDES.items():
        if env_name in env:
            values[section][key] = env[env_name]

    sw_version = resolve_sw_version(values["gateway"].get("sw_version_override"))
    return GatewayConfig(
        gateway_id=int(values["gateway"]["gateway_id"]),
        latitude=float(values["gateway"]["latitude"]),
        longitude=float(values["gateway"]["longitude"]),
        wisun_ipv6=str(values["gateway"].get("wisun_ipv6", "::")),
        sw_version=sw_version,
        coap=CoapConfig(
            bind_host=str(values["coap"].get("bind_host", "::")),
            port=int(values["coap"].get("port", 5683)),
            resource_path=str(values["coap"].get("resource_path", "/uplink")),
            duplicate_cache_ttl_seconds=float(values["coap"].get("duplicate_cache_ttl_seconds", 60.0)),
            duplicate_cache_max_entries=int(values["coap"].get("duplicate_cache_max_entries", 1024)),
            downlink_port=int(values["coap"].get("downlink_port", 5683)),
            downlink_resource_path=str(values["coap"].get("downlink_resource_path", "/downlink")),
            ack_timeout_seconds=float(values["coap"].get("ack_timeout_seconds", 2.0)),
            max_retransmit=int(values["coap"].get("max_retransmit", 4)),
        ),
        backhaul=BackhaulConfig(
            base_url=str(values["backhaul"].get("base_url", DEFAULT_BACKHAUL_BASE_URL)),
            http_timeout_seconds=float(values["backhaul"].get("http_timeout_seconds", 5.0)),
            registration_retry_base_delay_seconds=int(
                values["backhaul"].get("registration_retry_base_delay_seconds", 5)
            ),
            registration_retry_max_delay_seconds=int(
                values["backhaul"].get("registration_retry_max_delay_seconds", 300)
            ),
            uplink_retry_base_delay_seconds=int(
                values["backhaul"].get("uplink_retry_base_delay_seconds", 5)
            ),
            uplink_retry_max_delay_seconds=int(
                values["backhaul"].get("uplink_retry_max_delay_seconds", 300)
            ),
        ),
        runtime=RuntimeConfig(
            db_path=Path(values["runtime"]["db_path"]),
            log_level=str(values["runtime"].get("log_level", "INFO")),
            worker_poll_interval_seconds=float(
                values["runtime"].get("worker_poll_interval_seconds", 1.0)
            ),
        ),
        backhaul_version=int(values["gateway"].get("backhaul_version", 1)),
        node_packet_version=int(values["gateway"].get("node_packet_version", 1)),
        http_api=HttpApiConfig(
            bind_host=str(values["http_api"].get("bind_host", "::")),
            port=int(values["http_api"].get("port", 8081)),
            max_request_body_bytes=int(values["http_api"].get("max_request_body_bytes", 65536)),
        ),
    )
