from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlmodel import Session, select

from database import get_session
from models import Door, Permission
from dependencies import get_current_admin
from security import generate_api_key

router = APIRouter(prefix="/api/doors", tags=["doors"], dependencies=[Depends(get_current_admin)])


class DoorCreate(BaseModel):
    name: str
    location: Optional[str] = None
    description: Optional[str] = None
    active: bool = True


class DoorUpdate(BaseModel):
    name: Optional[str] = None
    location: Optional[str] = None
    description: Optional[str] = None
    active: Optional[bool] = None


@router.get("", response_model=List[Door])
def list_doors(session: Session = Depends(get_session)):
    return session.exec(select(Door).order_by(Door.name)).all()


@router.post("", response_model=Door)
def create_door(body: DoorCreate, session: Session = Depends(get_session)):
    door = Door(**body.model_dump(), api_key=generate_api_key())
    session.add(door)
    session.commit()
    session.refresh(door)
    return door


@router.patch("/{door_id}", response_model=Door)
def update_door(door_id: int, body: DoorUpdate, session: Session = Depends(get_session)):
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    for key, value in body.model_dump(exclude_unset=True).items():
        setattr(door, key, value)
    session.add(door)
    session.commit()
    session.refresh(door)
    return door


@router.post("/{door_id}/rotate-key", response_model=Door)
def rotate_key(door_id: int, session: Session = Depends(get_session)):
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    door.api_key = generate_api_key()
    session.add(door)
    session.commit()
    session.refresh(door)
    return door


@router.delete("/{door_id}")
def delete_door(door_id: int, session: Session = Depends(get_session)):
    door = session.get(Door, door_id)
    if not door:
        raise HTTPException(status_code=404, detail="Door not found")
    for permission in session.exec(select(Permission).where(Permission.door_id == door_id)).all():
        session.delete(permission)
    session.delete(door)
    session.commit()
    return {"ok": True}
