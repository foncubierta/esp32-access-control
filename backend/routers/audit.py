from typing import List, Optional
from datetime import datetime
from fastapi import APIRouter, Depends
from sqlmodel import Session, select, or_

from database import get_session
from models import AuditLog
from dependencies import get_current_admin

router = APIRouter(prefix="/api/audit-log", tags=["audit"], dependencies=[Depends(get_current_admin)])


@router.get("", response_model=List[AuditLog])
def list_audit_log(
    actor: Optional[str] = None,
    action: Optional[str] = None,
    entity_type: Optional[str] = None,
    entity_id: Optional[int] = None,
    q: Optional[str] = None,  # free-text search over summary/details/entity_label
    since: Optional[datetime] = None,
    until: Optional[datetime] = None,
    limit: int = 200,
    session: Session = Depends(get_session),
):
    query = select(AuditLog)
    if actor:
        query = query.where(AuditLog.actor == actor)
    if action:
        query = query.where(AuditLog.action == action)
    if entity_type:
        query = query.where(AuditLog.entity_type == entity_type)
    if entity_id is not None:
        query = query.where(AuditLog.entity_id == entity_id)
    if since is not None:
        query = query.where(AuditLog.created_at >= since)
    if until is not None:
        query = query.where(AuditLog.created_at <= until)
    if q:
        like = f"%{q}%"
        query = query.where(
            or_(
                AuditLog.summary.ilike(like),
                AuditLog.details.ilike(like),
                AuditLog.entity_label.ilike(like),
            )
        )
    query = query.order_by(AuditLog.created_at.desc()).limit(min(limit, 1000))
    return session.exec(query).all()
