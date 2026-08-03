from typing import List, Optional
from datetime import datetime, timezone
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlmodel import Session, select

import audit
import enrollment
import licensing
from database import get_session
from models import Door, Permission, AdminUser
from dependencies import get_current_admin
from security import generate_api_key

router = APIRouter(prefix="/api/doors", tags=["doors"], dependencies=[Depends(get_current_admin)])

DOOR_MODES = {"auto", "open", "closed", "identify"}
DOOR_FIELD_LABELS = {
    "name": "Nombre",
    "location": "Ubicación",
    "description": "Descripción",
    "active": "Activa",
    "mode": "Modo",
}

# The node's heartbeat is the tightest guaranteed update to last_seen (fixed
# 60s in the firmware, unlike the admin-configurable sync interval) — 2x
# that tolerates one missed beat plus some jitter before flagging offline.
ONLINE_THRESHOLD_S = 120


def _validate_mode(mode: Optional[str]):
    if mode is not None and mode not in DOOR_MODES:
        raise HTTPException(status_code=400, detail=f"mode must be one of {sorted(DOOR_MODES)}")


class DoorCreate(BaseModel):
    name: str
    location: Optional[str] = None
    description: Optional[str] = None
    active: bool = True


class DoorUpdate(BaseModel):
    name: Optional[str] = None
    location: Optional[str] = None
    description: Optional[str] = None
    active: Optional[bool] = None
    mode: Optional[str] = None


class DoorOut(BaseModel):
    id: int
    name: str
    location: Optional[str] = None
    description: Optional[str] = None
    api_key: str
    active: bool
    mode: str
    trigger_seq: int
    last_seen: Optional[datetime] = None
    created_at: datetime
    online: bool  # last_seen within ONLINE_THRESHOLD_S — not a stored column


def _is_online(last_seen: Optional[datetime]) -> bool:
    if last_seen is None:
        return False
    if last_seen.tzinfo is None:
        last_seen = last_seen.replace(tzinfo=timezone.utc)
    return (datetime.now(timezone.utc) - last_seen).total_seconds() < ONLINE_THRESHOLD_S


def _door_out(door: Door) -> DoorOut:
    return DoorOut(**door.model_dump(), online=_is_online(door.last_seen))


@router.get("", response_model=List[DoorOut])
def list_doors(session: Session = Depends(get_session)):
    doors = session.exec(select(Door).order_by(Door.name)).all()
    return [_door_out(door) for door in doors]


@router.post("", response_model=DoorOut)
def create_door(body: DoorCreate, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)):
    status = licensing.get_status(session)
    max_doors = status["max_doors"] if status["valid"] else 0
    used_doors = len(session.exec(select(Door).where(Door.active == True)).all())  # noqa: E712
    if used_doors >= max_doors:
        detail = (
            "No hay ninguna licencia válida instalada — no se pueden crear puertas."
            if max_doors == 0
            else f"Se alcanzó el límite de la licencia ({max_doors} puerta(s)). Instala una licencia con más puertas para añadir otra."
        )
        raise HTTPException(status_code=402, detail=detail)
    door = Door(**body.model_dump(), api_key=generate_api_key())
    session.add(door)
    session.commit()
    session.refresh(door)
    audit.log(session, admin.username, "created", "door", f"Creó la puerta «{door.name}»", entity_id=door.id, entity_label=door.name)
    return _door_out(door)


@router.patch("/{door_id}", response_model=DoorOut)
def update_door(
    door_id: int, body: DoorUpdate, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)
):
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    _validate_mode(body.mode)
    updates = body.model_dump(exclude_unset=True)
    before = {key: getattr(door, key) for key in updates}
    for key, value in updates.items():
        setattr(door, key, value)
    session.add(door)
    session.commit()
    session.refresh(door)
    changes = audit.describe_changes(before, updates, DOOR_FIELD_LABELS)
    if changes:
        audit.log(
            session, admin.username, "updated", "door", f"Editó la puerta «{door.name}»",
            entity_id=door.id, entity_label=door.name, details=changes,
        )
    return _door_out(door)


@router.post("/{door_id}/rotate-key", response_model=DoorOut)
def rotate_key(door_id: int, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)):
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    door.api_key = generate_api_key()
    session.add(door)
    session.commit()
    session.refresh(door)
    audit.log(
        session, admin.username, "key_rotated", "door", f"Rotó la API key de «{door.name}» (el nodo dejará de sincronizar hasta reconfigurarlo)",
        entity_id=door.id, entity_label=door.name,
    )
    return _door_out(door)


@router.post("/{door_id}/trigger", response_model=DoorOut)
def trigger_door(door_id: int, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)):
    """Guard-initiated one-off relay pulse, independent of mode/credentials.
    Bumps a counter the node picks up on its next mode poll (a few seconds
    later) and fires once — there's no direct connection to the node to
    push this immediately, it's a pull architecture end to end."""
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    door.trigger_seq += 1
    session.add(door)
    session.commit()
    session.refresh(door)
    audit.log(
        session, admin.username, "manual_trigger", "door", f"Envió una apertura manual a «{door.name}»",
        entity_id=door.id, entity_label=door.name,
    )
    return _door_out(door)


@router.post("/{door_id}/enroll/arm")
def arm_enroll(door_id: int, session: Session = Depends(get_session)):
    """Arms a one-shot "read the next card" session for this door's reader.
    The node picks this up on its next mode poll (a few seconds) and, on
    the very next scan, reports the raw value back — see enrollment.py for
    why this doesn't touch the database."""
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    enrollment.arm(door_id)
    return {"ok": True}


@router.get("/{door_id}/enroll/status")
def enroll_status(door_id: int, session: Session = Depends(get_session)):
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    return enrollment.status(door_id)


@router.delete("/{door_id}/enroll")
def disarm_enroll(door_id: int, session: Session = Depends(get_session)):
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    enrollment.disarm(door_id)
    return {"ok": True}


@router.delete("/{door_id}")
def delete_door(door_id: int, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)):
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    name = door.name
    for permission in session.exec(select(Permission).where(Permission.door_id == door_id)).all():
        session.delete(permission)
    session.delete(door)
    session.commit()
    audit.log(session, admin.username, "deleted", "door", f"Eliminó la puerta «{name}»", entity_id=door_id, entity_label=name)
    return {"ok": True}
