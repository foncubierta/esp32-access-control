from sqlmodel import Session, SQLModel, create_engine
from sqlalchemy import text
from pathlib import Path
import os

DB_URL = os.getenv("DATABASE_URL", "sqlite:///./data/access_control.db")
Path("./data").mkdir(exist_ok=True)

engine = create_engine(DB_URL, echo=False, connect_args={"check_same_thread": False})

# Columns added after initial release — migrate if missing
_MIGRATIONS = [
    "ALTER TABLE door ADD COLUMN mode TEXT NOT NULL DEFAULT 'auto'",
    "ALTER TABLE accesslog ADD COLUMN door_mode TEXT",
    "ALTER TABLE door ADD COLUMN trigger_seq INTEGER NOT NULL DEFAULT 0",
    "ALTER TABLE user ADD COLUMN dni TEXT",
    "ALTER TABLE user ADD COLUMN address TEXT",
    "ALTER TABLE user ADD COLUMN photo_path TEXT",
    "ALTER TABLE credential ADD COLUMN group_id INTEGER REFERENCES credentialgroup(id)",
]


def create_db():
    SQLModel.metadata.create_all(engine)
    _run_migrations()


def _run_migrations():
    with engine.connect() as conn:
        for stmt in _MIGRATIONS:
            try:
                conn.execute(text(stmt))
                conn.commit()
            except Exception:
                pass  # column already exists


def get_session():
    with Session(engine) as session:
        yield session
