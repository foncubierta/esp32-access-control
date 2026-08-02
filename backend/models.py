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
    notes: Optional[str] = None
    active: bool = Field(default=True)
    created_at: datetime = Field(default_factory=datetime.utcnow)


class Credential(SQLModel, table=True):
    """An RFID tag / PIN / NFC id belonging to a user. The raw value is never stored,
    only a SHA-256 hash — nodes hash what they read locally and compare hashes."""
    id: Optional[int] = Field(default=None, primary_key=True)
    user_id: int = Field(foreign_key="user.id", index=True)
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


class AccessLog(SQLModel, table=True):
    """Access attempt reported by a node, synced up in batches."""
    id: Optional[int] = Field(default=None, primary_key=True)
    door_id: int = Field(foreign_key="door.id", index=True)
    credential_id: Optional[int] = Field(default=None, foreign_key="credential.id", index=True)
    raw_value_hash: Optional[str] = None  # kept even when credential is unknown, for audit
    result: str  # granted | denied
    reason: Optional[str] = None  # unknown_credential | inactive | expired | schedule | no_permission | door_inactive
    event_time: datetime
    received_at: datetime = Field(default_factory=datetime.utcnow)
