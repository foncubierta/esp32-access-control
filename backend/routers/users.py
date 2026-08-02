from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlmodel import Session, select

from database import get_session
from models import User, Credential, Permission
from dependencies import get_current_admin

router = APIRouter(prefix="/api/users", tags=["users"], dependencies=[Depends(get_current_admin)])


class UserCreate(BaseModel):
    full_name: str
    email: Optional[str] = None
    phone: Optional[str] = None
    notes: Optional[str] = None
    active: bool = True


class UserUpdate(BaseModel):
    full_name: Optional[str] = None
    email: Optional[str] = None
    phone: Optional[str] = None
    notes: Optional[str] = None
    active: Optional[bool] = None


@router.get("", response_model=List[User])
def list_users(session: Session = Depends(get_session)):
    return session.exec(select(User).order_by(User.full_name)).all()


@router.post("", response_model=User)
def create_user(body: UserCreate, session: Session = Depends(get_session)):
    user = User(**body.model_dump())
    session.add(user)
    session.commit()
    session.refresh(user)
    return user


@router.get("/{user_id}", response_model=User)
def get_user(user_id: int, session: Session = Depends(get_session)):
    user = session.get(User, user_id)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    return user


@router.patch("/{user_id}", response_model=User)
def update_user(user_id: int, body: UserUpdate, session: Session = Depends(get_session)):
    user = session.get(User, user_id)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    for key, value in body.model_dump(exclude_unset=True).items():
        setattr(user, key, value)
    session.add(user)
    session.commit()
    session.refresh(user)
    return user


@router.delete("/{user_id}")
def delete_user(user_id: int, session: Session = Depends(get_session)):
    user = session.get(User, user_id)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    credentials = session.exec(select(Credential).where(Credential.user_id == user_id)).all()
    for credential in credentials:
        for permission in session.exec(select(Permission).where(Permission.credential_id == credential.id)).all():
            session.delete(permission)
        session.delete(credential)
    session.delete(user)
    session.commit()
    return {"ok": True}
