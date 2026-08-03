from typing import List, Optional
from datetime import datetime
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlmodel import Session, select

import audit
from database import get_session
from models import Credential, Permission, User, CredentialGroup, AdminUser
from dependencies import get_current_admin
from security import hash_credential_value, preview_value

router = APIRouter(prefix="/api/credentials", tags=["credentials"], dependencies=[Depends(get_current_admin)])

FIELD_LABELS = {
    "label": "Etiqueta",
    "active": "Activa",
    "valid_from": "Válida desde",
    "valid_until": "Válida hasta",
}


def _credential_label(session: Session, credential: Credential) -> str:
    owner = session.get(User, credential.user_id)
    owner_name = owner.full_name if owner else f"usuario #{credential.user_id}"
    return f"{credential.label or 'credencial'} ({owner_name})"


def _group_name(session: Session, group_id) -> str:
    if group_id is None:
        return "sin grupo"
    group = session.get(CredentialGroup, group_id)
    return group.name if group else f"#{group_id}"


class CredentialCreate(BaseModel):
    user_id: int
    group_id: Optional[int] = None
    type: str = "rfid"
    label: Optional[str] = None
    value: str  # raw value — hashed before storage, never persisted or returned as-is
    active: bool = True
    valid_from: Optional[datetime] = None
    valid_until: Optional[datetime] = None


class CredentialUpdate(BaseModel):
    group_id: Optional[int] = None
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
def create_credential(
    body: CredentialCreate, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)
):
    data = body.model_dump(exclude={"value"})
    credential = Credential(
        **data,
        value_hash=hash_credential_value(body.value),
        value_preview=preview_value(body.value),
    )
    session.add(credential)
    session.commit()
    session.refresh(credential)
    label = _credential_label(session, credential)
    audit.log(session, admin.username, "created", "credential", f"Creó la credencial «{label}»", entity_id=credential.id, entity_label=label)
    return credential


@router.patch("/{credential_id}", response_model=Credential)
def update_credential(
    credential_id: int,
    body: CredentialUpdate,
    admin: AdminUser = Depends(get_current_admin),
    session: Session = Depends(get_session),
):
    credential = session.get(Credential, credential_id)
    if not credential:
        raise HTTPException(status_code=404, detail="Credential not found")
    data = body.model_dump(exclude_unset=True)
    raw_value = data.pop("value", None)
    before = {key: getattr(credential, key) for key in data}
    group_changed = "group_id" in data and before.get("group_id") != data["group_id"]
    old_group_name = _group_name(session, before.get("group_id")) if group_changed else None
    for key, value in data.items():
        setattr(credential, key, value)
    if raw_value:
        credential.value_hash = hash_credential_value(raw_value)
        credential.value_preview = preview_value(raw_value)
    session.add(credential)
    session.commit()
    session.refresh(credential)

    parts = []
    if group_changed:
        parts.append(f"Grupo: {old_group_name} → {_group_name(session, credential.group_id)}")
    other_changes = audit.describe_changes(before, {k: v for k, v in data.items() if k != "group_id"}, FIELD_LABELS)
    if other_changes:
        parts.append(other_changes)
    if raw_value:
        parts.append("Valor rotado")
    if parts:
        label = _credential_label(session, credential)
        audit.log(
            session, admin.username, "updated", "credential", f"Editó la credencial «{label}»",
            entity_id=credential.id, entity_label=label, details="; ".join(parts),
        )
    return credential


@router.delete("/{credential_id}")
def delete_credential(
    credential_id: int, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)
):
    credential = session.get(Credential, credential_id)
    if not credential:
        raise HTTPException(status_code=404, detail="Credential not found")
    label = _credential_label(session, credential)
    for permission in session.exec(select(Permission).where(Permission.credential_id == credential_id)).all():
        session.delete(permission)
    session.delete(credential)
    session.commit()
    audit.log(session, admin.username, "deleted", "credential", f"Eliminó la credencial «{label}»", entity_id=credential_id, entity_label=label)
    return {"ok": True}
