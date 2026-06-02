from __future__ import annotations
import json
from pathlib import Path

_CONFIG_FILE = Path(__file__).parent / "belt_config.json"

# GT2 20-tooth default: 20 teeth × 2 mm pitch = 40 mm/rev
_DEFAULT_MM_PER_REV = 40.0


class BeltConfig:
    def __init__(self) -> None:
        self.mm_per_rev: float = _DEFAULT_MM_PER_REV
        self._load()

    def _load(self) -> None:
        if _CONFIG_FILE.exists():
            try:
                data = json.loads(_CONFIG_FILE.read_text())
                self.mm_per_rev = float(data.get("mm_per_rev", _DEFAULT_MM_PER_REV))
            except Exception:
                pass

    def save(self) -> None:
        _CONFIG_FILE.write_text(json.dumps({"mm_per_rev": self.mm_per_rev}, indent=2))

    def deg_to_mm(self, deg: float) -> float:
        return deg * (self.mm_per_rev / 360.0)

    def mm_to_deg(self, mm: float) -> float:
        return mm * (360.0 / self.mm_per_rev)

    @property
    def mm_per_deg(self) -> float:
        return self.mm_per_rev / 360.0
