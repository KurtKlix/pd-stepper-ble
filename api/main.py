from __future__ import annotations
import argparse
import logging
import sys
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from bleak.exc import BleakError

from belt_config import BeltConfig
from ble_manager import BLEManager
from models import (
    BeltConfigRequest,
    ConnectRequest,
    ConfigureRequest,
    DeviceInfo,
    EnableRequest,
    EndstopModeRequest,
    MoveAbsoluteMmRequest,
    MoveAbsoluteRequest,
    MoveRelativeMmRequest,
    MoveRelativeRequest,
    SetVelocityMmRequest,
    SetVelocityRequest,
    StatusResponse,
)
from websocket_manager import WebSocketManager

logging.basicConfig(level=logging.INFO, format="%(levelname)s  %(name)s  %(message)s")

# ── App setup ─────────────────────────────────────────────────────────────────

ws_manager  = WebSocketManager()
belt_config = BeltConfig()
ble_manager = BLEManager(ws_manager, belt_config)


@asynccontextmanager
async def lifespan(_app: FastAPI):
    yield
    # Clean up BLE on shutdown
    if ble_manager.connected:
        await ble_manager.disconnect()


app = FastAPI(title="PD-Stepper API", version="1.0.0", lifespan=lifespan)

STATIC_DIR = Path(__file__).parent / "static"
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

# ── Helpers ───────────────────────────────────────────────────────────────────

def _ble_error(exc: Exception) -> HTTPException:
    return HTTPException(status_code=503, detail=str(exc))

# ── Routes: discovery & connection ───────────────────────────────────────────

@app.get("/api/scan", response_model=list[DeviceInfo])
async def scan(duration: float = 5.0):
    """Scan for nearby PD-Stepper BLE devices."""
    try:
        return await ble_manager.scan(duration)
    except Exception as exc:
        raise _ble_error(exc)


@app.post("/api/connect")
async def connect(req: ConnectRequest):
    """Connect to a PD-Stepper by BLE address."""
    try:
        await ble_manager.connect(req.address)
        return {"ok": True, "address": req.address}
    except Exception as exc:
        raise _ble_error(exc)


@app.post("/api/disconnect")
async def disconnect():
    """Disconnect from the current device."""
    await ble_manager.disconnect()
    return {"ok": True}


@app.get("/api/status", response_model=StatusResponse)
async def status():
    """Connection state and latest motor status snapshot."""
    return StatusResponse(
        connected=ble_manager.connected,
        state=ble_manager.state.name,
        address=ble_manager.address,
        last_status=ble_manager.last_status,
    )


@app.get("/api/config")
async def config():
    """Read device configuration from CONFIG characteristic."""
    try:
        return await ble_manager.read_config()
    except BleakError as exc:
        raise _ble_error(exc)

# ── Routes: motion ────────────────────────────────────────────────────────────

@app.post("/api/move/absolute")
async def move_absolute(req: MoveAbsoluteRequest):
    try:
        await ble_manager.send_command({"cmd": "deg", "val": req.degrees})
        return {"ok": True}
    except BleakError as exc:
        raise _ble_error(exc)


@app.post("/api/move/relative")
async def move_relative(req: MoveRelativeRequest):
    try:
        await ble_manager.send_command({"cmd": "deg_rel", "val": req.degrees})
        return {"ok": True}
    except BleakError as exc:
        raise _ble_error(exc)


@app.post("/api/velocity")
async def set_velocity(req: SetVelocityRequest):
    try:
        await ble_manager.send_command({"cmd": "vel", "val": req.dps})
        return {"ok": True}
    except BleakError as exc:
        raise _ble_error(exc)


@app.post("/api/stop")
async def stop():
    try:
        await ble_manager.send_command({"cmd": "stop"})
        return {"ok": True}
    except BleakError as exc:
        raise _ble_error(exc)


@app.post("/api/home")
async def home():
    """Start endstop homing sequence."""
    try:
        await ble_manager.send_command({"cmd": "home"})
        return {"ok": True}
    except BleakError as exc:
        raise _ble_error(exc)

# ── Routes: configuration ─────────────────────────────────────────────────────

@app.post("/api/configure")
async def configure(req: ConfigureRequest):
    """Apply partial configuration update."""
    field_map = {
        "current_ma":      "current",
        "microsteps":      "microsteps",
        "speed_sps":       "speed",
        "closed_loop_type":"closed_loop_type",
        "mapping_direction":"mappingDirection",
    }
    try:
        for field, cmd in field_map.items():
            val = getattr(req, field)
            if val is not None:
                await ble_manager.send_command({"cmd": cmd, "val": val})
        return {"ok": True}
    except BleakError as exc:
        raise _ble_error(exc)


@app.post("/api/enable")
async def enable(req: EnableRequest):
    try:
        await ble_manager.send_command({"cmd": "enable", "val": 1 if req.enabled else 0})
        return {"ok": True}
    except BleakError as exc:
        raise _ble_error(exc)


# ── Belt / linear unit configuration ─────────────────────────────────────────

@app.get("/api/belt")
async def get_belt():
    """Return current belt configuration and conversion factor."""
    return {
        "mm_per_rev": belt_config.mm_per_rev,
        "mm_per_deg": belt_config.mm_per_deg,
    }


@app.post("/api/belt")
async def set_belt(req: BeltConfigRequest):
    """Configure linear travel per motor revolution. Persists across restarts."""
    belt_config.mm_per_rev = req.mm_per_rev
    belt_config.save()
    return {"ok": True, "mm_per_rev": belt_config.mm_per_rev}


# ── mm-based motion endpoints ─────────────────────────────────────────────────

@app.post("/api/move/absolute_mm")
async def move_absolute_mm(req: MoveAbsoluteMmRequest):
    """Move to absolute position in millimetres."""
    try:
        deg = belt_config.mm_to_deg(req.mm)
        await ble_manager.send_command({"cmd": "deg", "val": deg})
        return {"ok": True, "mm": req.mm, "deg": round(deg, 4)}
    except BleakError as exc:
        raise _ble_error(exc)


@app.post("/api/move/relative_mm")
async def move_relative_mm(req: MoveRelativeMmRequest):
    """Move relative distance in millimetres."""
    try:
        deg = belt_config.mm_to_deg(req.mm)
        await ble_manager.send_command({"cmd": "deg_rel", "val": deg})
        return {"ok": True, "mm": req.mm, "deg": round(deg, 4)}
    except BleakError as exc:
        raise _ble_error(exc)


@app.post("/api/velocity_mm")
async def set_velocity_mm(req: SetVelocityMmRequest):
    """Set velocity in mm/s. Negative = reverse. 0 = stop."""
    try:
        dps = belt_config.mm_to_deg(req.mm_per_s)
        await ble_manager.send_command({"cmd": "vel", "val": dps})
        return {"ok": True, "mm_per_s": req.mm_per_s, "dps": round(dps, 4)}
    except BleakError as exc:
        raise _ble_error(exc)


@app.post("/api/endstop_mode")
async def endstop_mode(req: EndstopModeRequest):
    """Set endstop switch type: normally_closed=false → NO, true → NC. No reflash needed."""
    try:
        await ble_manager.send_command({"cmd": "endstop_mode", "val": 1 if req.normally_closed else 0})
        return {"ok": True, "mode": "NC" if req.normally_closed else "NO"}
    except BleakError as exc:
        raise _ble_error(exc)

# ── WebSocket ─────────────────────────────────────────────────────────────────

@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws_manager.connect(ws)
    try:
        while True:
            # Keep connection alive; data flows server→client via broadcasts
            await ws.receive_text()
    except WebSocketDisconnect:
        ws_manager.disconnect(ws)

# ── Static / UI ───────────────────────────────────────────────────────────────

@app.get("/")
async def root():
    return FileResponse(str(STATIC_DIR / "index.html"))

# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import uvicorn

    parser = argparse.ArgumentParser(description="PD-Stepper BLE API")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()

    uvicorn.run("main:app", host=args.host, port=args.port, log_level="info")
