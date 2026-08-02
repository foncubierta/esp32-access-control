from typing import List, Optional
from datetime import datetime, timezone
from fastapi import APIRouter, Depends
from pydantic import BaseModel
from sqlmodel import Session, select

from database import get_session
from models import Door, Credential, Permission, AccessLog
from dependencies import get_current_door

router = APIRouter(prefix="/api/node", tags=["node"])


class SyncCredential(BaseModel):
    credential_id: int
    value_hash: str
    days_of_week: Optional[str] = None
    time_start: Optional[str] = None
    time_end: Optional[str] = None
    valid_from: Optional[datetime] = None
    valid_until: Optional[datetime] = None


class SyncResponse(BaseModel):
    door_id: int
    door_name: str
    door_active: bool
    door_mode: str
    trigger_seq: int
    generated_at: datetime
    credentials: List[SyncCredential]


@router.get("/sync", response_model=SyncResponse)
def sync(door: Door = Depends(get_current_door), session: Session = Depends(get_session)):
    """Called periodically by the node. Returns the full set of credentials
    currently allowed through this door, so the node can cache it locally and
    keep enforcing access even if the network drops before the next sync.
    An inactive door gets an empty list, which is what actually locks it down."""
    door.last_seen = datetime.now(timezone.utc)
    session.add(door)
    session.commit()

    credentials: List[SyncCredential] = []
    if door.active:
        rows = session.exec(
            select(Credential, Permission)
            .join(Permission, Permission.credential_id == Credential.id)
            .where(Permission.door_id == door.id)
            .where(Permission.active == True)  # noqa: E712
            .where(Credential.active == True)  # noqa: E712
        ).all()
        credentials = [
            SyncCredential(
                credential_id=credential.id,
                value_hash=credential.value_hash,
                days_of_week=permission.days_of_week,
                time_start=permission.time_start,
                time_end=permission.time_end,
                valid_from=credential.valid_from,
                valid_until=credential.valid_until,
            )
            for credential, permission in rows
        ]

    return SyncResponse(
        door_id=door.id,
        door_name=door.name,
        door_active=door.active,
        door_mode=door.mode,
        trigger_seq=door.trigger_seq,
        generated_at=datetime.now(timezone.utc),
        credentials=credentials,
    )


class ModeResponse(BaseModel):
    door_id: int
    door_active: bool
    door_mode: str
    trigger_seq: int


@router.get("/mode", response_model=ModeResponse)
def get_mode(door: Door = Depends(get_current_door)):
    """Lightweight, high-frequency poll so a guard flipping the door mode —
    or firing a one-off manual open — from the web takes effect in seconds,
    without re-fetching the whole credential list on every check the way
    /sync does. trigger_seq lets the node detect a manual "open now" click:
    it bumps on every POST /api/doors/:id/trigger, and the node fires a
    single pulse whenever it sees the value change since its last poll."""
    return ModeResponse(door_id=door.id, door_active=door.active, door_mode=door.mode, trigger_seq=door.trigger_seq)


class LogEntry(BaseModel):
    value_hash: Optional[str] = None
    credential_id: Optional[int] = None
    result: str  # granted | denied — whether the credential itself would have had access
    reason: Optional[str] = None
    door_mode: Optional[str] = None  # what the door was set to when this happened
    event_time: datetime


class LogsBatch(BaseModel):
    entries: List[LogEntry]


@router.post("/logs")
def upload_logs(
    body: LogsBatch, door: Door = Depends(get_current_door), session: Session = Depends(get_session)
):
    """Nodes queue access events locally and flush them here in batches —
    keeps the audit trail intact even after a period offline."""
    for entry in body.entries:
        session.add(
            AccessLog(
                door_id=door.id,
                credential_id=entry.credential_id,
                raw_value_hash=entry.value_hash,
                result=entry.result,
                reason=entry.reason,
                door_mode=entry.door_mode,
                event_time=entry.event_time,
            )
        )
    session.commit()
    return {"ok": True, "received": len(body.entries)}


@router.post("/heartbeat")
def heartbeat(door: Door = Depends(get_current_door), session: Session = Depends(get_session)):
    door.last_seen = datetime.now(timezone.utc)
    session.add(door)
    session.commit()
    return {"ok": True}
