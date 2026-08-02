from fastapi import Depends, Header, HTTPException, status
from sqlmodel import Session, select
import jwt

from database import get_session
from models import AdminUser, Door
from auth import decode_access_token


def get_current_admin(
    authorization: str = Header(default=None),
    session: Session = Depends(get_session),
) -> AdminUser:
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Not authenticated")
    token = authorization.removeprefix("Bearer ").strip()
    try:
        username = decode_access_token(token)
    except jwt.PyJWTError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid or expired token")
    admin = session.exec(select(AdminUser).where(AdminUser.username == username)).first()
    if not admin:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid or expired token")
    return admin


def get_current_door(
    x_api_key: str = Header(default=None),
    session: Session = Depends(get_session),
) -> Door:
    if not x_api_key:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Missing X-Api-Key")
    door = session.exec(select(Door).where(Door.api_key == x_api_key)).first()
    if not door:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid API key")
    # inactive doors still authenticate, so /api/node/sync can hand them an
    # empty credential list — that's what makes disabling a door actually lock it.
    return door
