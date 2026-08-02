"""In-memory, per-door "read a card now" sessions used by the admin panel's
"Leer tarjeta" button (Credenciales > Nueva credencial). Deliberately not a
DB table: the whole point of the project's security model is that a raw
credential value is never persisted — this holds it just long enough for
one browser poll to pick it up, then it's gone. It already crosses the LAN
in plaintext the same way a manually-typed value does when the admin
submits the credential form; this just automates typing it in.

Single-process in-memory state — fine for the size of deployment this
project targets (one backend instance). A restart clears any pending
session, which is harmless: the admin just clicks "Leer tarjeta" again.
"""
from datetime import datetime, timedelta, timezone
from typing import Optional

ARM_TIMEOUT = timedelta(seconds=60)

_state: dict[int, dict] = {}


def _expired(entry: dict) -> bool:
    return datetime.now(timezone.utc) - entry["armed_at"] > ARM_TIMEOUT


def arm(door_id: int) -> None:
    _state[door_id] = {"armed_at": datetime.now(timezone.utc), "captured": None}


def disarm(door_id: int) -> None:
    _state.pop(door_id, None)


def is_armed(door_id: int) -> bool:
    """True while the node should report the next card it reads — i.e. armed
    and nothing captured yet. Once a value lands, this flips false so the
    node stops reporting further scans until the admin arms it again."""
    entry = _state.get(door_id)
    if not entry:
        return False
    if _expired(entry):
        _state.pop(door_id, None)
        return False
    return entry["captured"] is None


def status(door_id: int) -> dict:
    entry = _state.get(door_id)
    if not entry or _expired(entry):
        _state.pop(door_id, None)
        return {"armed": False, "captured": None}
    return {"armed": entry["captured"] is None, "captured": entry["captured"]}


def report(door_id: int, value: str, bit_count: Optional[int]) -> bool:
    """Called by the node once it reads a card while armed. Only the first
    report per arm sticks — later scans in the same session are dropped."""
    entry = _state.get(door_id)
    if not entry or _expired(entry) or entry["captured"] is not None:
        return False
    entry["captured"] = {
        "value": value,
        "bit_count": bit_count,
        "captured_at": datetime.now(timezone.utc).isoformat(),
    }
    return True
