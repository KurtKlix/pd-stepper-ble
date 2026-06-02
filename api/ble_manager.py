from __future__ import annotations
import asyncio
import json
import logging
from enum import Enum, auto

from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

from belt_config import BeltConfig
from models import DeviceInfo
from websocket_manager import WebSocketManager

logger = logging.getLogger(__name__)

SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DC4179"
CMD_UUID     = "6E400002-B5A3-F393-E0A9-E50E24DC4179"
STATUS_UUID  = "6E400003-B5A3-F393-E0A9-E50E24DC4179"
CONFIG_UUID  = "6E400004-B5A3-F393-E0A9-E50E24DC4179"


class ConnectionState(Enum):
    DISCONNECTED = auto()
    SCANNING     = auto()
    CONNECTING   = auto()
    CONNECTED    = auto()


class BLEManager:
    def __init__(self, ws_manager: WebSocketManager, belt: BeltConfig) -> None:
        self._ws = ws_manager
        self._belt = belt
        self._client: BleakClient | None = None
        self._address: str | None = None
        self._state = ConnectionState.DISCONNECTED
        self._last_status: dict = {}
        self._reconnect_task: asyncio.Task | None = None
        self._loop: asyncio.AbstractEventLoop | None = None

    # ── Public state ──────────────────────────────────────────────────────────

    @property
    def state(self) -> ConnectionState:
        return self._state

    @property
    def address(self) -> str | None:
        return self._address

    @property
    def last_status(self) -> dict:
        return self._last_status

    @property
    def connected(self) -> bool:
        return self._state == ConnectionState.CONNECTED

    # ── Scan ─────────────────────────────────────────────────────────────────

    async def scan(self, duration: float = 5.0) -> list[DeviceInfo]:
        self._state = ConnectionState.SCANNING
        try:
            # return_adv=True gives (BLEDevice, AdvertisementData) so we can get RSSI
            results: dict = await BleakScanner.discover(
                timeout=duration, return_adv=True, service_uuids=[SERVICE_UUID]
            )
            return [
                DeviceInfo(
                    address=device.address,
                    name=device.name or "PD-Stepper",
                    rssi=adv.rssi if adv.rssi is not None else -100,
                )
                for device, adv in results.values()
            ]
        finally:
            if self._state == ConnectionState.SCANNING:
                self._state = ConnectionState.DISCONNECTED

    # ── Connect / disconnect ──────────────────────────────────────────────────

    async def connect(self, address: str) -> None:
        self._address = address
        self._state   = ConnectionState.CONNECTING
        self._loop    = asyncio.get_running_loop()

        self._client = BleakClient(
            address,
            disconnected_callback=self._on_disconnect,
            timeout=10.0,
        )
        await self._client.connect()
        await self._client.start_notify(STATUS_UUID, self._on_notification)
        self._state = ConnectionState.CONNECTED
        logger.info("Connected to %s", address)

    async def disconnect(self) -> None:
        self._cancel_reconnect()
        self._address = None
        if self._client and self._client.is_connected:
            await self._client.stop_notify(STATUS_UUID)
            await self._client.disconnect()
        self._client = None
        self._state  = ConnectionState.DISCONNECTED

    # ── Commands ──────────────────────────────────────────────────────────────

    async def send_command(self, payload: dict) -> None:
        self._require_connected()
        data = json.dumps(payload).encode()
        await self._client.write_gatt_char(CMD_UUID, data, response=False)  # type: ignore[union-attr]

    async def read_config(self) -> dict:
        self._require_connected()
        raw = await self._client.read_gatt_char(CONFIG_UUID)  # type: ignore[union-attr]
        return json.loads(raw.decode())

    def _require_connected(self) -> None:
        if not self.connected or self._client is None:
            raise BleakError("Not connected to a device")

    # ── BLE notification handler ──────────────────────────────────────────────

    def _on_notification(self, _sender: int, data: bytearray) -> None:
        try:
            msg = json.loads(data.decode())
            # Inject mm equivalents so all clients get linear units automatically
            if "pos_deg" in msg:
                msg["pos_mm"] = round(self._belt.deg_to_mm(msg["pos_deg"]), 3)
            if "target_deg" in msg:
                msg["target_mm"] = round(self._belt.deg_to_mm(msg["target_deg"]), 3)
            if "vel_dps" in msg:
                msg["vel_mm_s"] = round(self._belt.deg_to_mm(msg["vel_dps"]), 3)
            self._last_status = msg
            if self._loop and not self._loop.is_closed():
                asyncio.run_coroutine_threadsafe(
                    self._ws.broadcast(msg), self._loop
                )
        except Exception as exc:
            logger.warning("Notification parse error: %s", exc)

    # ── Disconnect / reconnect ────────────────────────────────────────────────

    def _on_disconnect(self, _client: BleakClient) -> None:
        logger.warning("BLE disconnected from %s", self._address)
        self._state = ConnectionState.DISCONNECTED
        if self._address and self._loop and not self._loop.is_closed():
            self._reconnect_task = asyncio.run_coroutine_threadsafe(
                self._reconnect_loop(), self._loop
            )

    async def _reconnect_loop(self) -> None:
        delay = 1.0
        while self._address and self._state != ConnectionState.CONNECTED:
            logger.info("Reconnecting in %.0fs...", delay)
            await asyncio.sleep(delay)
            try:
                await self.connect(self._address)
                logger.info("Reconnected successfully")
                return
            except Exception as exc:
                logger.warning("Reconnect failed: %s", exc)
                delay = min(delay * 2.0, 30.0)

    def _cancel_reconnect(self) -> None:
        if self._reconnect_task and not self._reconnect_task.done():
            self._reconnect_task.cancel()
        self._reconnect_task = None
