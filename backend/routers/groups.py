from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlmodel import Session, select

from database import get_session
from models import CredentialGroup, GroupPermission, Credential, Door
from dependencies import get_current_admin

router = APIRouter(prefix="/api/groups", tags=["groups"], dependencies=[Depends(get_current_admin)])


class GroupCreate(BaseModel):
    name: str
    description: Optional[str] = None
    active: bool = True


class GroupUpdate(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None
    active: Optional[bool] = None


@router.get("", response_model=List[CredentialGroup])
def list_groups(session: Session = Depends(get_session)):
    return session.exec(select(CredentialGroup).order_by(CredentialGroup.name)).all()


@router.post("", response_model=CredentialGroup)
def create_group(body: GroupCreate, session: Session = Depends(get_session)):
    group = CredentialGroup(**body.model_dump())
    session.add(group)
    session.commit()
    session.refresh(group)
    return group


@router.patch("/{group_id}", response_model=CredentialGroup)
def update_group(group_id: int, body: GroupUpdate, session: Session = Depends(get_session)):
    group = session.get(CredentialGroup, group_id)
    if not group:
        raise HTTPException(status_code=404, detail="Group not found")
    for key, value in body.model_dump(exclude_unset=True).items():
        setattr(group, key, value)
    session.add(group)
    session.commit()
    session.refresh(group)
    return group


@router.delete("/{group_id}")
def delete_group(group_id: int, session: Session = Depends(get_session)):
    group = session.get(CredentialGroup, group_id)
    if not group:
        raise HTTPException(status_code=404, detail="Group not found")
    for permission in session.exec(select(GroupPermission).where(GroupPermission.group_id == group_id)).all():
        session.delete(permission)
    for credential in session.exec(select(Credential).where(Credential.group_id == group_id)).all():
        credential.group_id = None
        session.add(credential)
    session.delete(group)
    session.commit()
    return {"ok": True}


class GroupPermissionCreate(BaseModel):
    group_id: int
    door_id: int
    days_of_week: Optional[str] = None  # "0,1,2,3,4" Mon=0..Sun=6, None = every day
    time_start: Optional[str] = None    # "HH:MM"
    time_end: Optional[str] = None      # "HH:MM"
    active: bool = True


class GroupPermissionUpdate(BaseModel):
    days_of_week: Optional[str] = None
    time_start: Optional[str] = None
    time_end: Optional[str] = None
    active: Optional[bool] = None


@router.get("/permissions", response_model=List[GroupPermission])
def list_group_permissions(
    group_id: Optional[int] = None,
    door_id: Optional[int] = None,
    session: Session = Depends(get_session),
):
    query = select(GroupPermission)
    if group_id is not None:
        query = query.where(GroupPermission.group_id == group_id)
    if door_id is not None:
        query = query.where(GroupPermission.door_id == door_id)
    return session.exec(query).all()


@router.post("/permissions", response_model=GroupPermission)
def create_group_permission(body: GroupPermissionCreate, session: Session = Depends(get_session)):
    if not session.get(CredentialGroup, body.group_id):
        raise HTTPException(status_code=404, detail="Group not found")
    if not session.get(Door, body.door_id):
        raise HTTPException(status_code=404, detail="Door not found")
    permission = GroupPermission(**body.model_dump())
    session.add(permission)
    session.commit()
    session.refresh(permission)
    return permission


@router.patch("/permissions/{permission_id}", response_model=GroupPermission)
def update_group_permission(permission_id: int, body: GroupPermissionUpdate, session: Session = Depends(get_session)):
    permission = session.get(GroupPermission, permission_id)
    if not permission:
        raise HTTPException(status_code=404, detail="Group permission not found")
    for key, value in body.model_dump(exclude_unset=True).items():
        setattr(permission, key, value)
    session.add(permission)
    session.commit()
    session.refresh(permission)
    return permission


@router.delete("/permissions/{permission_id}")
def delete_group_permission(permission_id: int, session: Session = Depends(get_session)):
    permission = session.get(GroupPermission, permission_id)
    if not permission:
        raise HTTPException(status_code=404, detail="Group permission not found")
    session.delete(permission)
    session.commit()
    return {"ok": True}
