from __future__ import annotations

from dataclasses import dataclass


class DownlinkValidationError(ValueError):
    """Raised when a downlink HTTP request body is invalid."""


def _require_int(data: dict, key: str, *, minimum: int, maximum: int) -> int:
    if key not in data:
        raise DownlinkValidationError(f"missing field: {key}")
    value = data[key]
    if isinstance(value, bool) or not isinstance(value, int):
        raise DownlinkValidationError(f"field {key} must be an integer")
    if value < minimum or value > maximum:
        raise DownlinkValidationError(f"field {key} out of range")
    return value


@dataclass(frozen=True)
class NNTableRequest:
    target_node_id: int
    neighbor_node_ids: list[int]


@dataclass(frozen=True)
class TimeSyncRequest:
    target_node_id: int
    epoch: int


@dataclass(frozen=True)
class ConfigUpdateRequest:
    target_node_id: int
    config_id: int
    l2_temp_thresh: int
    l2_humidity_thresh: int
    l2_voc_thresh: int
    l3_temp_thresh: int
    l3_humidity_thresh: int
    l3_voc_thresh: int
    l4_voc_thresh: int
    l5_voc_thresh: int
    l5_pm25_thresh: int


def validate_nn_table_request(data: dict) -> NNTableRequest:
    target_node_id = _require_int(data, "target_node_id", minimum=0, maximum=0xFFFF)
    if "neighbor_node_ids" not in data:
        raise DownlinkValidationError("missing field: neighbor_node_ids")
    neighbors = data["neighbor_node_ids"]
    if not isinstance(neighbors, list):
        raise DownlinkValidationError("field neighbor_node_ids must be an array")
    if len(neighbors) > 0xFF:
        raise DownlinkValidationError("neighbor_node_ids may not contain more than 255 entries")
    validated = []
    for index, neighbor in enumerate(neighbors):
        if isinstance(neighbor, bool) or not isinstance(neighbor, int):
            raise DownlinkValidationError(f"neighbor_node_ids[{index}] must be an integer")
        if neighbor < 0 or neighbor > 0xFFFF:
            raise DownlinkValidationError(f"neighbor_node_ids[{index}] out of range")
        validated.append(neighbor)
    return NNTableRequest(target_node_id=target_node_id, neighbor_node_ids=validated)


def validate_time_sync_request(data: dict) -> TimeSyncRequest:
    return TimeSyncRequest(
        target_node_id=_require_int(data, "target_node_id", minimum=0, maximum=0xFFFF),
        epoch=_require_int(data, "epoch", minimum=0, maximum=0xFFFFFFFF),
    )


def validate_config_update_request(data: dict) -> ConfigUpdateRequest:
    return ConfigUpdateRequest(
        target_node_id=_require_int(data, "target_node_id", minimum=0, maximum=0xFFFF),
        config_id=_require_int(data, "config_id", minimum=0, maximum=0xFFFFFFFF),
        l2_temp_thresh=_require_int(data, "l2_temp_thresh", minimum=-0x8000, maximum=0x7FFF),
        l2_humidity_thresh=_require_int(data, "l2_humidity_thresh", minimum=0, maximum=0xFFFF),
        l2_voc_thresh=_require_int(data, "l2_voc_thresh", minimum=0, maximum=0xFFFF),
        l3_temp_thresh=_require_int(data, "l3_temp_thresh", minimum=-0x8000, maximum=0x7FFF),
        l3_humidity_thresh=_require_int(data, "l3_humidity_thresh", minimum=0, maximum=0xFFFF),
        l3_voc_thresh=_require_int(data, "l3_voc_thresh", minimum=0, maximum=0xFFFF),
        l4_voc_thresh=_require_int(data, "l4_voc_thresh", minimum=0, maximum=0xFFFF),
        l5_voc_thresh=_require_int(data, "l5_voc_thresh", minimum=0, maximum=0xFFFF),
        l5_pm25_thresh=_require_int(data, "l5_pm25_thresh", minimum=0, maximum=0xFFFF),
    )
