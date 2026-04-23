from __future__ import annotations

import math
from dataclasses import dataclass


@dataclass(frozen=True)
class NeighborComputation:
    node_id: int
    neighbor_node_ids: list[int]


class NeighborComputeService:
    def __init__(self, *, radius_meters: float, max_neighbors: int | None) -> None:
        self._radius_meters = radius_meters
        self._max_neighbors = max_neighbors

    @property
    def radius_meters(self) -> float:
        return self._radius_meters

    @property
    def max_neighbors(self) -> int | None:
        return self._max_neighbors

    def compute(self, nodes) -> dict[int, NeighborComputation]:
        eligible_nodes = [
            node
            for node in nodes
            if node.latitude is not None and node.longitude is not None
        ]
        computed = {}
        for node in eligible_nodes:
            ranked_neighbors = []
            for neighbor in eligible_nodes:
                if neighbor.node_id == node.node_id:
                    continue
                distance = _haversine_meters(
                    node.latitude,
                    node.longitude,
                    neighbor.latitude,
                    neighbor.longitude,
                )
                if distance > self._radius_meters:
                    continue
                ranked_neighbors.append((distance, neighbor.node_id))
            ranked_neighbors.sort(key=lambda item: (item[0], item[1]))
            neighbor_ids = [neighbor_node_id for _distance, neighbor_node_id in ranked_neighbors]
            if self._max_neighbors is not None:
                neighbor_ids = neighbor_ids[: self._max_neighbors]
            computed[node.node_id] = NeighborComputation(node_id=node.node_id, neighbor_node_ids=neighbor_ids)
        return computed


def _haversine_meters(latitude_a: float, longitude_a: float, latitude_b: float, longitude_b: float) -> float:
    earth_radius_meters = 6_371_000.0
    lat_a = math.radians(latitude_a)
    lon_a = math.radians(longitude_a)
    lat_b = math.radians(latitude_b)
    lon_b = math.radians(longitude_b)
    delta_lat = lat_b - lat_a
    delta_lon = lon_b - lon_a
    haversine = (
        math.sin(delta_lat / 2.0) ** 2
        + math.cos(lat_a) * math.cos(lat_b) * math.sin(delta_lon / 2.0) ** 2
    )
    return 2.0 * earth_radius_meters * math.asin(math.sqrt(haversine))
