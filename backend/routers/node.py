from typing import List, Optional
from datetime import datetime, timezone
from fastapi import APIRouter, Depends
from pydantic import BaseModel
from sqlmodel import Session, select

import enrollment
from database import get_session
from models import Door, Credential, Permission, CredentialGroup, GroupPermission, AccessLog
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
    enroll_armed: bool
    generated_at: datetime
    credentials: List[SyncCredential]


@router.get("/sync", response_model=SyncResponse)
def sync(door: Door = Depends(get_current_door), session: Session = Depends(get_session)):
    """Called periodically by the node. Returns the full set of credentials
    currently allowed through this door, so the node can cache it locally and
    keep enforcing access even if the network drops before the next sync.
    An inactive door gets an empty list, which is what actually locks it down.

    A credential's access is the union of two independent grants — direct
    Permission on the credential itself, and GroupPermission via whatever
    CredentialGroup it belongs to. The same credential can show up twice
    here (once per grant, possibly with different schedules); the node
    caches every row and grants access if *any* matching entry currently
    allows it — see AccessController::evaluate() in the firmware."""
    door.last_seen = datetime.now(timezone.utc)
    session.add(door)
    session.commit()

    credentials: List[SyncCredential] = []
    if door.active:
        direct_rows = session.exec(
            select(Credential, Permission)
            .join(Permission, Permission.credential_id == Credential.id)
            .where(Permission.door_id == door.id)
            .where(Permission.active == True)  # noqa: E712
            .where(Credential.active == True)  # noqa: E712
        ).all()
        credentials.extend(
            SyncCredential(
                credential_id=credential.id,
                value_hash=credential.value_hash,
                days_of_week=permission.days_of_week,
                time_start=permission.time_start,
                time_end=permission.time_end,
                valid_from=credential.valid_from,
                valid_until=credential.valid_until,
            )
            for credential, permission in direct_rows
        )

        group_rows = session.exec(
            select(Credential, GroupPermission)
            .join(CredentialGroup, CredentialGroup.id == Credential.group_id)
            .join(GroupPermission, GroupPermission.group_id == CredentialGroup.id)
            .where(GroupPermission.door_id == door.id)
            .where(GroupPermission.active == True)  # noqa: E712
            .where(CredentialGroup.active == True)  # noqa: E712
            .where(Credential.active == True)  # noqa: E712
        ).all()
        credentials.extend(
            SyncCredential(
                credential_id=credential.id,
                value_hash=credential.value_hash,
                days_of_week=group_permission.days_of_week,
                time_start=group_permission.time_start,
                time_end=group_permission.time_end,
                valid_from=credential.valid_from,
                valid_until=credential.valid_until,
            )
            for credential, group_permission in group_rows
        )

    return SyncResponse(
        door_id=door.id,
        door_name=door.name,
        door_active=door.active,
        door_mode=door.mode,
        trigger_seq=door.trigger_seq,
        enroll_armed=enrollment.is_armed(door.id),
        generated_at=datetime.now(timezone.utc),
        credentials=credentials,
    )


class ModeResponse(BaseModel):
    door_id: int
    door_active: bool
    door_mode: str
    trigger_seq: int
    enroll_armed: bool


@router.get("/mode", response_model=ModeResponse)
def get_mode(door: Door = Depends(get_current_door)):
    """Lightweight, high-frequency poll so a guard flipping the door mode —
    or firing a one-off manual open — from the web takes effect in seconds,
    without re-fetching the whole credential list on every check the way
    /sync does. trigger_seq lets the node detect a manual "open now" click:
    it bumps on every POST /api/doors/:id/trigger, and the node fires a
    single pulse whenever it sees the value change since its last poll.
    enroll_armed tells it to report the very next card it reads verbatim to
    POST /api/node/enroll, for the "Leer tarjeta" button on Credenciales."""
    return ModeResponse(
        door_id=door.id,
        door_active=door.active,
        door_mode=door.mode,
        trigger_seq=door.trigger_seq,
        enroll_armed=enrollment.is_armed(door.id),
    )


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


class EnrollReport(BaseModel):
    value: str  # raw canonical value, e.g. "W26:0A3F91" — never stored, just relayed
    bit_count: Optional[int] = None


@router.post("/enroll")
def report_enrollment(body: EnrollReport, door: Door = Depends(get_current_door)):
    """The node calls this once, right after reading a card while
    enroll_armed was true. Held in memory only (see enrollment.py) for the
    admin's browser to pick up via GET /api/doors/:id/enroll/status —
    dropped silently if nothing is actually armed (session expired, admin
    already cancelled, etc.)."""
    enrollment.report(door.id, body.value, body.bit_count)
    return {"ok": True}


@router.post("/heartbeat")
def heartbeat(door: Door = Depends(get_current_door), session: Session = Depends(get_session)):
    door.last_seen = datetime.now(timezone.utc)
    session.add(door)
    session.commit()
    return {"ok": True}


class SensorReport(BaseModel):
    open: bool
    forced: bool = False  # only meaningful when open=True — no preceding granted access/manual trigger


@router.post("/sensor")
def report_sensor(body: SensorReport, door: Door = Depends(get_current_door), session: Session = Depends(get_session)):
    """Pushed by the node the moment its door-position sensor changes state
    — edge-triggered, not polled, so a forced-open reaches the panel within
    moments instead of waiting for the next mode/sync cycle. `forced` is
    always cleared back to false as soon as the sensor reports closed
    again; the "held open too long" alert isn't decided here at all — it's
    computed on read from sensor_since, same as the online/offline status
    (see doors.py:_door_alert)."""
    now = datetime.now(timezone.utc)
    if door.sensor_open != body.open:
        door.sensor_since = now  # only reset the clock on an actual transition, not a duplicate report
    door.sensor_open = body.open
    door.sensor_forced = body.forced if body.open else False
    door.last_seen = now
    session.add(door)
    session.commit()

    if body.open and body.forced:
        session.add(
            AccessLog(door_id=door.id, result="denied", reason="door_forced", door_mode=door.mode, event_time=now)
        )
        session.commit()

    return {"ok": True}
