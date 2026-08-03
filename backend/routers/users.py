from pathlib import Path
from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException, UploadFile, File
from fastapi.responses import FileResponse
from pydantic import BaseModel
from sqlmodel import Session, select

import audit
from database import get_session
from models import User, Credential, Permission, AdminUser
from dependencies import get_current_admin

router = APIRouter(prefix="/api/users", tags=["users"], dependencies=[Depends(get_current_admin)])

PHOTOS_DIR = Path("./data/photos")
PHOTOS_DIR.mkdir(parents=True, exist_ok=True)
ALLOWED_PHOTO_EXTENSIONS = {".jpg", ".jpeg", ".png", ".webp"}

FIELD_LABELS = {
    "full_name": "Nombre",
    "email": "Email",
    "phone": "Teléfono",
    "dni": "DNI",
    "address": "Dirección",
    "notes": "Notas",
    "active": "Activo",
}


class UserCreate(BaseModel):
    full_name: str
    email: Optional[str] = None
    phone: Optional[str] = None
    dni: Optional[str] = None
    address: Optional[str] = None
    notes: Optional[str] = None
    active: bool = True


class UserUpdate(BaseModel):
    full_name: Optional[str] = None
    email: Optional[str] = None
    phone: Optional[str] = None
    dni: Optional[str] = None
    address: Optional[str] = None
    notes: Optional[str] = None
    active: Optional[bool] = None


@router.get("", response_model=List[User])
def list_users(session: Session = Depends(get_session)):
    return session.exec(select(User).order_by(User.full_name)).all()


@router.post("", response_model=User)
def create_user(body: UserCreate, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)):
    user = User(**body.model_dump())
    session.add(user)
    session.commit()
    session.refresh(user)
    audit.log(session, admin.username, "created", "user", f"Creó al usuario «{user.full_name}»", entity_id=user.id, entity_label=user.full_name)
    return user


@router.get("/{user_id}", response_model=User)
def get_user(user_id: int, session: Session = Depends(get_session)):
    user = session.get(User, user_id)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    return user


@router.patch("/{user_id}", response_model=User)
def update_user(
    user_id: int, body: UserUpdate, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)
):
    user = session.get(User, user_id)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    updates = body.model_dump(exclude_unset=True)
    before = {key: getattr(user, key) for key in updates}
    for key, value in updates.items():
        setattr(user, key, value)
    session.add(user)
    session.commit()
    session.refresh(user)
    changes = audit.describe_changes(before, updates, FIELD_LABELS)
    if changes:
        audit.log(
            session, admin.username, "updated", "user", f"Editó al usuario «{user.full_name}»",
            entity_id=user.id, entity_label=user.full_name, details=changes,
        )
    return user


@router.delete("/{user_id}")
def delete_user(user_id: int, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)):
    user = session.get(User, user_id)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    full_name = user.full_name
    credentials = session.exec(select(Credential).where(Credential.user_id == user_id)).all()
    for credential in credentials:
        for permission in session.exec(select(Permission).where(Permission.credential_id == credential.id)).all():
            session.delete(permission)
        session.delete(credential)
    if user.photo_path:
        (PHOTOS_DIR / user.photo_path).unlink(missing_ok=True)
    session.delete(user)
    session.commit()
    audit.log(
        session, admin.username, "deleted", "user",
        f"Eliminó al usuario «{full_name}» (con sus credenciales y permisos)",
        entity_id=user_id, entity_label=full_name,
    )
    return {"ok": True}


@router.post("/{user_id}/photo", response_model=User)
async def upload_photo(
    user_id: int,
    file: UploadFile = File(...),
    admin: AdminUser = Depends(get_current_admin),
    session: Session = Depends(get_session),
):
    user = session.get(User, user_id)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    if not (file.content_type or "").startswith("image/"):
        raise HTTPException(status_code=400, detail="El archivo debe ser una imagen")
    ext = Path(file.filename or "").suffix.lower()
    if ext not in ALLOWED_PHOTO_EXTENSIONS:
        ext = ".jpg"
    content = await file.read()

    if user.photo_path:
        (PHOTOS_DIR / user.photo_path).unlink(missing_ok=True)
    filename = f"{user_id}{ext}"
    (PHOTOS_DIR / filename).write_bytes(content)

    user.photo_path = filename
    session.add(user)
    session.commit()
    session.refresh(user)
    audit.log(
        session, admin.username, "updated", "user", f"Subió una foto para «{user.full_name}»",
        entity_id=user.id, entity_label=user.full_name,
    )
    return user


@router.get("/{user_id}/photo")
def get_photo(user_id: int, session: Session = Depends(get_session)):
    user = session.get(User, user_id)
    if not user or not user.photo_path:
        raise HTTPException(status_code=404, detail="No photo")
    path = PHOTOS_DIR / user.photo_path
    if not path.exists():
        raise HTTPException(status_code=404, detail="No photo")
    return FileResponse(path)


@router.delete("/{user_id}/photo", response_model=User)
def delete_photo(user_id: int, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)):
    user = session.get(User, user_id)
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    if user.photo_path:
        (PHOTOS_DIR / user.photo_path).unlink(missing_ok=True)
        user.photo_path = None
        session.add(user)
        session.commit()
        session.refresh(user)
        audit.log(
            session, admin.username, "updated", "user", f"Quitó la foto de «{user.full_name}»",
            entity_id=user.id, entity_label=user.full_name,
        )
    return user
