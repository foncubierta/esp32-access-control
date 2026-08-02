from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlmodel import Session, select

from database import get_session
from models import Permission, Credential, Door
from dependencies import get_current_admin

router = APIRouter(prefix="/api/permissions", tags=["permissions"], dependencies=[Depends(get_current_admin)])


class PermissionCreate(BaseModel):
    credential_id: int
    door_id: int
    days_of_week: Optional[str] = None  # "0,1,2,3,4" Mon=0..Sun=6, None = every day
    time_start: Optional[str] = None    # "HH:MM"
    time_end: Optional[str] = None      # "HH:MM"
    active: bool = True


class PermissionUpdate(BaseModel):
    days_of_week: Optional[str] = None
    time_start: Optional[str] = None
    time_end: Optional[str] = None
    active: Optional[bool] = None


@router.get("", response_model=List[Permission])
def list_permissions(
    door_id: Optional[int] = None,
    credential_id: Optional[int] = None,
    user_id: Optional[int] = None,
    session: Session = Depends(get_session),
):
    query = select(Permission)
    if door_id is not None:
        query = query.where(Permission.door_id == door_id)
    if credential_id is not None:
        query = query.where(Permission.credential_id == credential_id)
    if user_id is not None:
        query = query.join(Credential, Credential.id == Permission.credential_id).where(
            Credential.user_id == user_id
        )
    return session.exec(query).all()


@router.post("", response_model=Permission)
def create_permission(body: PermissionCreate, session: Session = Depends(get_session)):
    if not session.get(Credential, body.credential_id):
        raise HTTPException(status_code=404, detail="Credential not found")
    if not session.get(Door, body.door_id):
        raise HTTPException(status_code=404, detail="Door not found")
    permission = Permission(**body.model_dump())
    session.add(permission)
    session.commit()
    session.refresh(permission)
    return permission


@router.patch("/{permission_id}", response_model=Permission)
def update_permission(permission_id: int, body: PermissionUpdate, session: Session = Depends(get_session)):
    permission = session.get(Permission, permission_id)
    if not permission:
        raise HTTPException(status_code=404, detail="Permission not found")
    for key, value in body.model_dump(exclude_unset=True).items():
        setattr(permission, key, value)
    session.add(permission)
    session.commit()
    session.refresh(permission)
    return permission


@router.delete("/{permission_id}")
def delete_permission(permission_id: int, session: Session = Depends(get_session)):
    permission = session.get(Permission, permission_id)
    if not permission:
        raise HTTPException(status_code=404, detail="Permission not found")
    session.delete(permission)
    session.commit()
    return {"ok": True}
