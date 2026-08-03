from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel
from sqlmodel import Session, select

import audit
from database import get_session
from models import AdminUser
from security import verify_password, hash_password
from auth import create_access_token
from dependencies import get_current_admin

router = APIRouter(prefix="/api/auth", tags=["auth"])


class LoginRequest(BaseModel):
    username: str
    password: str


class LoginResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"


class AdminOut(BaseModel):
    id: int
    username: str


@router.post("/login", response_model=LoginResponse)
def login(body: LoginRequest, session: Session = Depends(get_session)):
    admin = session.exec(select(AdminUser).where(AdminUser.username == body.username)).first()
    if not admin or not verify_password(body.password, admin.password_hash):
        audit.log(session, body.username, "login_failed", "admin_account", f"Login fallido para «{body.username}»")
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid credentials")
    token = create_access_token(admin.username)
    audit.log(session, admin.username, "login_success", "admin_account", f"«{admin.username}» inició sesión", entity_id=admin.id, entity_label=admin.username)
    return LoginResponse(access_token=token)


@router.get("/me", response_model=AdminOut)
def me(admin: AdminUser = Depends(get_current_admin)):
    return AdminOut(id=admin.id, username=admin.username)


class ChangePasswordRequest(BaseModel):
    current_password: str
    new_password: str


@router.post("/change-password")
def change_password(
    body: ChangePasswordRequest,
    admin: AdminUser = Depends(get_current_admin),
    session: Session = Depends(get_session),
):
    if not verify_password(body.current_password, admin.password_hash):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="La contraseña actual no es correcta")
    if len(body.new_password) < 8:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="La nueva contraseña debe tener al menos 8 caracteres")
    admin.password_hash = hash_password(body.new_password)
    session.add(admin)
    session.commit()
    audit.log(
        session, admin.username, "password_changed", "admin_account", f"«{admin.username}» cambió su contraseña",
        entity_id=admin.id, entity_label=admin.username,
    )
    return {"ok": True}
