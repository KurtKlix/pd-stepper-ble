from __future__ import annotations
from pydantic import BaseModel, Field
from typing import Any


class ConnectRequest(BaseModel):
    address: str


class MoveAbsoluteRequest(BaseModel):
    degrees: float


class MoveRelativeRequest(BaseModel):
    degrees: float


class SetVelocityRequest(BaseModel):
    dps: float = Field(description="Degrees per second; negative = reverse; 0 = stop")


class ConfigureRequest(BaseModel):
    current_ma: int | None = None
    microsteps: int | None = None
    speed_sps: int | None = None
    closed_loop_type: int | None = None
    mapping_direction: int | None = None
    home_direction: int | None = None  # 1 or -1: direction motor travels to reach endstop


class EnableRequest(BaseModel):
    enabled: bool


class EndstopModeRequest(BaseModel):
    normally_closed: bool  # False = NO (default), True = NC


class BeltConfigRequest(BaseModel):
    mm_per_rev: float = Field(gt=0, description="Linear travel per motor revolution (mm)")


class MoveAbsoluteMmRequest(BaseModel):
    mm: float


class MoveRelativeMmRequest(BaseModel):
    mm: float


class SetVelocityMmRequest(BaseModel):
    mm_per_s: float  # negative = reverse


class DeviceInfo(BaseModel):
    address: str
    name: str
    rssi: int


class StatusResponse(BaseModel):
    connected: bool
    state: str
    address: str | None
    last_status: dict[str, Any]
