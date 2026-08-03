from typing import Optional
from sqlmodel import Field, SQLModel
from datetime import datetime


class AdminUser(SQLModel, table=True):
    id: Optional[int] = Field(default=None, primary_key=True)
    username: str = Field(index=True, unique=True)
    password_hash: str
    created_at: datetime = Field(default_factory=datetime.utcnow)


class User(SQLModel, table=True):
    """A person who may be granted credentials to one or more doors."""
    id: Optional[int] = Field(default=None, primary_key=True)
    full_name: str
    email: Optional[str] = None
    phone: Optional[str] = None
    dni: Optional[str] = None
    address: Optional[str] = None
    photo_path: Optional[str] = None  # filename under backend/data/photos/, served via /api/users/:id/photo
    notes: Optional[str] = None
    active: bool = Field(default=True)
    created_at: datetime = Field(default_factory=datetime.utcnow)


class CredentialGroup(SQLModel, table=True):
    """A named access profile (VIP, Mantenimiento, Zona A...). Every active
    credential assigned to a group inherits whatever doors the group has
    access to (GroupPermission), on top of any direct Permission it might
    also hold — a credential's real access is the union of both."""
    id: Optional[int] = Field(default=None, primary_key=True)
    name: str = Field(index=True, unique=True)
    description: Optional[str] = None
    active: bool = Field(default=True)
    created_at: datetime = Field(default_factory=datetime.utcnow)


class Credential(SQLModel, table=True):
    """An RFID tag / PIN / NFC id belonging to a user. The raw value is never stored,
    only a SHA-256 hash — nodes hash what they read locally and compare hashes."""
    id: Optional[int] = Field(default=None, primary_key=True)
    user_id: int = Field(foreign_key="user.id", index=True)
    group_id: Optional[int] = Field(default=None, foreign_key="credentialgroup.id", index=True)
    type: str = Field(default="rfid")  # rfid | pin | nfc
    label: Optional[str] = None
    value_hash: str = Field(index=True)
    value_preview: Optional[str] = None  # last 4 chars, display only, not sensitive enough to identify
    active: bool = Field(default=True)
    valid_from: Optional[datetime] = None
    valid_until: Optional[datetime] = None
    created_at: datetime = Field(default_factory=datetime.utcnow)


class Door(SQLModel, table=True):
    """A physical door/gate controlled by one ESP32 node."""
    id: Optional[int] = Field(default=None, primary_key=True)
    name: str
    location: Optional[str] = None  # building / floor
    description: Optional[str] = None
    api_key: str = Field(index=True, unique=True)
    active: bool = Field(default=True)
    mode: str = Field(default="auto")  # auto | open | closed | identify — set from the guard view
    trigger_seq: int = Field(default=0)  # bumped on each manual "open now" from the guard view
    last_seen: Optional[datetime] = None
    created_at: datetime = Field(default_factory=datetime.utcnow)


class Permission(SQLModel, table=True):
    """Grants a credential access to a door, optionally restricted to days/hours."""
    id: Optional[int] = Field(default=None, primary_key=True)
    credential_id: int = Field(foreign_key="credential.id", index=True)
    door_id: int = Field(foreign_key="door.id", index=True)
    days_of_week: Optional[str] = None  # comma separated, 0=Mon..6=Sun, None = every day
    time_start: Optional[str] = None  # "HH:MM", None = no lower bound
    time_end: Optional[str] = None    # "HH:MM", None = no upper bound
    active: bool = Field(default=True)
    created_at: datetime = Field(default_factory=datetime.utcnow)


class GroupPermission(SQLModel, table=True):
    """Grants every active credential in a CredentialGroup access to a Door,
    optionally restricted to days/hours — same shape as Permission, just
    scoped to a whole group instead of one credential."""
    id: Optional[int] = Field(default=None, primary_key=True)
    group_id: int = Field(foreign_key="credentialgroup.id", index=True)
    door_id: int = Field(foreign_key="door.id", index=True)
    days_of_week: Optional[str] = None  # comma separated, 0=Mon..6=Sun, None = every day
    time_start: Optional[str] = None  # "HH:MM", None = no lower bound
    time_end: Optional[str] = None    # "HH:MM", None = no upper bound
    active: bool = Field(default=True)
    created_at: datetime = Field(default_factory=datetime.utcnow)


class AuditLog(SQLModel, table=True):
    """Admin-panel activity trail — who changed what and when. Separate from
    AccessLog (which is node-reported physical access attempts): this is
    for troubleshooting questions like "who deactivated this credential" or
    "who opened door X manually just now", not "did this card get in"."""
    id: Optional[int] = Field(default=None, primary_key=True)
    actor: str  # admin username, or the attempted username for failed logins
    action: str  # created | updated | deleted | login_success | login_failed | password_changed | manual_trigger | key_rotated
    entity_type: str  # user | credential | credential_group | group_permission | door | permission | admin_account
    entity_id: Optional[int] = None
    entity_label: Optional[str] = None  # human-readable name snapshot — still readable after the entity is deleted
    summary: str  # one-liner for the list view, e.g. "Quitó acceso de 'Tarjeta Juan' a 'Puerta Norte'"
    details: Optional[str] = None  # optional extra context, e.g. field-level before → after
    created_at: datetime = Field(default_factory=datetime.utcnow)


class License(SQLModel, table=True):
    """The currently installed license token. Single-row table — installing
    a new license overwrites this one; what was installed before is still
    visible in AuditLog. The token is a signed JWT (see licensing.py) —
    nothing in it is trusted until the signature is verified against the
    vendor's public key."""
    id: Optional[int] = Field(default=None, primary_key=True)
    token: str
    updated_at: datetime = Field(default_factory=datetime.utcnow)


class AccessLog(SQLModel, table=True):
    """Access attempt reported by a node, synced up in batches."""
    id: Optional[int] = Field(default=None, primary_key=True)
    door_id: int = Field(foreign_key="door.id", index=True)
    credential_id: Optional[int] = Field(default=None, foreign_key="credential.id", index=True)
    raw_value_hash: Optional[str] = None  # kept even when credential is unknown, for audit
    result: str  # granted | denied — whether the credential itself would have had access
    reason: Optional[str] = None  # unknown_credential | inactive | expired | schedule | no_permission | door_inactive
    door_mode: Optional[str] = None  # auto | open | closed | identify — what the door was set to when this happened
    event_time: datetime
    received_at: datetime = Field(default_factory=datetime.utcnow)
