"""Offline, signed door-count licensing. The customer never has shell or
DB access to this server (vendor-managed deployment) — tampering with the
license row directly isn't a realistic threat, so the point of signing is
narrower: only the vendor (holder of the private key, generated and kept
in license-tool/, never committed to this repo) can produce a token this
backend will accept as valid, so the door limit can't be raised just by
editing a config file.

Private key: license-tool/ (vendor machine only, gitignored).
Public key: license_public_key.pem, committed here — verify only, never
signs anything."""
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

import jwt
from sqlmodel import Session, select

import audit
from models import Door, License

ALGORITHM = "EdDSA"
_PUBLIC_KEY_PATH = Path(__file__).parent / "license_public_key.pem"


def _public_key() -> Optional[str]:
    if not _PUBLIC_KEY_PATH.exists():
        return None
    return _PUBLIC_KEY_PATH.read_text()


def decode(token: str) -> dict:
    """Verifies signature + expiry and returns the claims. Raises on
    anything invalid (missing public key, bad signature, expired) —
    callers decide how to present that with describe_error()."""
    public_key = _public_key()
    if not public_key:
        raise ValueError("No hay clave pública de licencia instalada en el servidor")
    return jwt.decode(token, public_key, algorithms=[ALGORITHM])


def describe_error(err: Exception) -> str:
    if isinstance(err, jwt.ExpiredSignatureError):
        return "La licencia ha caducado"
    if isinstance(err, jwt.InvalidSignatureError):
        return "La firma de la licencia no es válida"
    if isinstance(err, jwt.PyJWTError):
        return "El token de licencia no es válido"
    return str(err)


def get_token(session: Session) -> Optional[str]:
    row = session.exec(select(License).order_by(License.id.desc())).first()
    return row.token if row else None


def save_token(session: Session, token: str) -> None:
    row = session.exec(select(License).order_by(License.id.desc())).first()
    if row:
        row.token = token
        row.updated_at = datetime.utcnow()
        session.add(row)
    else:
        session.add(License(token=token))
    session.commit()


def get_status(session: Session) -> dict:
    """Never raises — always a full status dict, valid=False on anything
    wrong, so callers can render/enforce uniformly. used_doors only counts
    active doors, matching what actually competes for the license limit."""
    token = get_token(session)
    used_doors = len(session.exec(select(Door).where(Door.active == True)).all())  # noqa: E712
    status = {
        "has_license": token is not None,
        "valid": False,
        "max_doors": 0,
        "used_doors": used_doors,
        "customer": None,
        "issued_at": None,
        "expires_at": None,
        "error": None,
    }
    if not token:
        status["error"] = "No hay ninguna licencia instalada"
        return status
    try:
        claims = decode(token)
    except Exception as err:
        status["error"] = describe_error(err)
        return status
    status["valid"] = True
    status["max_doors"] = int(claims.get("max_doors", 0))
    status["customer"] = claims.get("customer")
    if claims.get("iat"):
        status["issued_at"] = datetime.fromtimestamp(claims["iat"], tz=timezone.utc)
    if claims.get("exp"):
        status["expires_at"] = datetime.fromtimestamp(claims["exp"], tz=timezone.utc)
    return status


def enforce(session: Session, actor: str = "system") -> list[Door]:
    """Deactivates the newest active doors beyond the licensed max, if any.
    Called at startup and after every login, so a license that expired or
    got replaced with a smaller one gets enforced without an admin having
    to touch anything. Never reactivates doors on its own — that's always
    a deliberate admin action once back within the limit."""
    status = get_status(session)
    max_doors = status["max_doors"] if status["valid"] else 0
    active_doors = session.exec(
        select(Door).where(Door.active == True).order_by(Door.created_at)  # noqa: E712
    ).all()
    if len(active_doors) <= max_doors:
        return []
    to_deactivate = active_doors[max_doors:]  # keep the oldest max_doors, drop the newest
    for door in to_deactivate:
        door.active = False
        session.add(door)
    session.commit()
    names = ", ".join(f"«{d.name}»" for d in to_deactivate)
    reason = status["error"] or f"límite de licencia de {max_doors} puerta(s)"
    audit.log(
        session, actor, "license_enforced", "door",
        f"Desactivó {len(to_deactivate)} puerta(s) por {reason}: {names}",
        details=reason,
    )
    return to_deactivate
