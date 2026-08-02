from typing import List, Optional
from datetime import datetime
from fastapi import APIRouter, Depends
from sqlmodel import Session, select

from database import get_session
from models import AccessLog
from dependencies import get_current_admin

router = APIRouter(prefix="/api/logs", tags=["logs"], dependencies=[Depends(get_current_admin)])


@router.get("", response_model=List[AccessLog])
def list_logs(
    door_id: Optional[int] = None,
    credential_id: Optional[int] = None,
    result: Optional[str] = None,
    since: Optional[datetime] = None,
    limit: int = 200,
    session: Session = Depends(get_session),
):
    query = select(AccessLog)
    if door_id is not None:
        query = query.where(AccessLog.door_id == door_id)
    if credential_id is not None:
        query = query.where(AccessLog.credential_id == credential_id)
    if result is not None:
        query = query.where(AccessLog.result == result)
    if since is not None:
        query = query.where(AccessLog.event_time >= since)
    query = query.order_by(AccessLog.event_time.desc()).limit(min(limit, 1000))
    return session.exec(query).all()
