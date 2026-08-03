"""Admin activity trail. Routers call log() explicitly right after a
mutation commits — no ORM event magic, so it's obvious from reading a
router exactly what gets audited and with what wording. Kept as a
separate commit from the main change: simpler than threading it through
every existing session.commit() call, at the (accepted) cost that a
freak failure writing the audit row wouldn't roll back the real change."""
from typing import Optional

from sqlmodel import Session

from models import AuditLog


def _format_value(value) -> str:
    if value is None or value == "":
        return "—"
    if isinstance(value, bool):
        return "Sí" if value else "No"
    return str(value)


def describe_changes(before: dict, updates: dict, labels: Optional[dict] = None) -> Optional[str]:
    """Builds a human-readable "campo: antes → después" string for the
    fields actually present in `updates` that differ from `before`.
    Returns None if nothing actually changed (e.g. a PATCH that re-sent
    the same values)."""
    labels = labels or {}
    parts = []
    for key, new_value in updates.items():
        old_value = before.get(key)
        if old_value == new_value:
            continue
        label = labels.get(key, key)
        parts.append(f"{label}: {_format_value(old_value)} → {_format_value(new_value)}")
    return "; ".join(parts) if parts else None


def log(
    session: Session,
    actor: str,
    action: str,
    entity_type: str,
    summary: str,
    entity_id: Optional[int] = None,
    entity_label: Optional[str] = None,
    details: Optional[str] = None,
) -> None:
    session.add(
        AuditLog(
            actor=actor,
            action=action,
            entity_type=entity_type,
            entity_id=entity_id,
            entity_label=entity_label,
            summary=summary,
            details=details,
        )
    )
    session.commit()
