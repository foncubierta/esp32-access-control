import hashlib
import secrets

import bcrypt


def hash_password(raw: str) -> str:
    return bcrypt.hashpw(raw.encode("utf-8"), bcrypt.gensalt()).decode("utf-8")


def verify_password(raw: str, password_hash: str) -> bool:
    return bcrypt.checkpw(raw.encode("utf-8"), password_hash.encode("utf-8"))


def hash_credential_value(raw: str) -> str:
    """Credential values (RFID UID, PIN...) are normalized and hashed — the raw
    value is never persisted. Nodes hash what they read locally and compare hashes."""
    normalized = raw.strip().upper()
    return hashlib.sha256(normalized.encode("utf-8")).hexdigest()


def preview_value(raw: str) -> str:
    normalized = raw.strip().upper()
    return normalized[-4:] if len(normalized) > 4 else normalized


def generate_api_key() -> str:
    return secrets.token_hex(32)
