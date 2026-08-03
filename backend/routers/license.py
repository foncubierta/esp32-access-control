from datetime import datetime
from typing import Optional

from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlmodel import Session

import audit
import licensing
from database import get_session
from models import AdminUser
from dependencies import get_current_admin

router = APIRouter(prefix="/api/license", tags=["license"], dependencies=[Depends(get_current_admin)])


class LicenseStatus(BaseModel):
    has_license: bool
    valid: bool
    max_doors: int
    used_doors: int
    customer: Optional[str] = None
    issued_at: Optional[datetime] = None
    expires_at: Optional[datetime] = None
    error: Optional[str] = None


class LicenseInstall(BaseModel):
    token: str


@router.get("", response_model=LicenseStatus)
def get_license(session: Session = Depends(get_session)):
    return LicenseStatus(**licensing.get_status(session))


@router.put("", response_model=LicenseStatus)
def install_license(
    body: LicenseInstall, admin: AdminUser = Depends(get_current_admin), session: Session = Depends(get_session)
):
    try:
        licensing.decode(body.token)
    except Exception as err:
        raise HTTPException(status_code=400, detail=licensing.describe_error(err))

    licensing.save_token(session, body.token)
    deactivated = licensing.enforce(session, admin.username)
    status = licensing.get_status(session)

    summary = f"Instaló una licencia para {status['max_doors']} puerta(s)"
    if status.get("customer"):
        summary += f" ({status['customer']})"
    if deactivated:
        summary += f" — desactivó {len(deactivated)} puerta(s) por superar el nuevo límite"
    audit.log(session, admin.username, "license_installed", "license", summary)

    return LicenseStatus(**status)
