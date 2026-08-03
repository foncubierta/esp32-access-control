from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlmodel import Session, select

import audit
import enrollment
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


@router.get("", response_model=List[Door])
def list_doors(session: Session = Depends(get_session)):
    return session.exec(select(Door).order_by(Door.name)).all()


@router.post("", response_model=Door)
def create_door(body: DoorCreate, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)):
    door = Door(**body.model_dump(), api_key=generate_api_key())
    session.add(door)
    session.commit()
    session.refresh(door)
    audit.log(session, admin.username, "created", "door", f"Creó la puerta «{door.name}»", entity_id=door.id, entity_label=door.name)
    return door


@router.patch("/{door_id}", response_model=Door)
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
    return door


@router.post("/{door_id}/rotate-key", response_model=Door)
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
    return door


@router.post("/{door_id}/trigger", response_model=Door)
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
    return door


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
