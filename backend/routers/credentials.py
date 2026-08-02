from typing import List, Optional
from datetime import datetime
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlmodel import Session, select

from database import get_session
from models import Credential, Permission
from dependencies import get_current_admin
from security import hash_credential_value, preview_value

router = APIRouter(prefix="/api/credentials", tags=["credentials"], dependencies=[Depends(get_current_admin)])


class CredentialCreate(BaseModel):
    user_id: int
    type: str = "rfid"
    label: Optional[str] = None
    value: str  # raw value — hashed before storage, never persisted or returned as-is
    active: bool = True
    valid_from: Optional[datetime] = None
    valid_until: Optional[datetime] = None


class CredentialUpdate(BaseModel):
    label: Optional[str] = None
    active: Optional[bool] = None
    valid_from: Optional[datetime] = None
    valid_until: Optional[datetime] = None
    value: Optional[str] = None  # re-provide to rotate the credential's value


@router.get("", response_model=List[Credential])
def list_credentials(user_id: Optional[int] = None, session: Session = Depends(get_session)):
    query = select(Credential)
    if user_id is not None:
        query = query.where(Credential.user_id == user_id)
    return session.exec(query.order_by(Credential.created_at.desc())).all()


@router.post("", response_model=Credential)
def create_credential(body: CredentialCreate, session: Session = Depends(get_session)):
    data = body.model_dump(exclude={"value"})
    credential = Credential(
        **data,
        value_hash=hash_credential_value(body.value),
        value_preview=preview_value(body.value),
    )
    session.add(credential)
    session.commit()
    session.refresh(credential)
    return credential


@router.patch("/{credential_id}", response_model=Credential)
def update_credential(credential_id: int, body: CredentialUpdate, session: Session = Depends(get_session)):
    credential = session.get(Credential, credential_id)
    if not credential:
        raise HTTPException(status_code=404, detail="Credential not found")
    data = body.model_dump(exclude_unset=True)
    raw_value = data.pop("value", None)
    for key, value in data.items():
        setattr(credential, key, value)
    if raw_value:
        credential.value_hash = hash_credential_value(raw_value)
        credential.value_preview = preview_value(raw_value)
    session.add(credential)
    session.commit()
    session.refresh(credential)
    return credential


@router.delete("/{credential_id}")
def delete_credential(credential_id: int, session: Session = Depends(get_session)):
    credential = session.get(Credential, credential_id)
    if not credential:
        raise HTTPException(status_code=404, detail="Credential not found")
    for permission in session.exec(select(Permission).where(Permission.credential_id == credential_id)).all():
        session.delete(permission)
    session.delete(credential)
    session.commit()
    return {"ok": True}
