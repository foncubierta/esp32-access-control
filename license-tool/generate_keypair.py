#!/usr/bin/env python3
"""Generates the vendor's Ed25519 keypair for signing licenses.

Run this ONCE, keep the private key somewhere safe (password manager,
offline drive) and NEVER commit it or hand it to a customer. The public
key gets copied into backend/license_public_key.pem in the deployable
repo so the server can verify licenses without being able to forge them.

    python3 generate_keypair.py [output_dir]

Writes private_key.pem and public_key.pem into output_dir (default:
./keys, gitignored).
"""
import sys
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey


def main():
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "keys"
    out_dir.mkdir(parents=True, exist_ok=True)

    private_path = out_dir / "private_key.pem"
    public_path = out_dir / "public_key.pem"

    if private_path.exists() or public_path.exists():
        print(f"Ya existen claves en {out_dir} — bórralas a mano si quieres regenerarlas.")
        sys.exit(1)

    private_key = Ed25519PrivateKey.generate()
    public_key = private_key.public_key()

    private_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    public_pem = public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )

    private_path.write_bytes(private_pem)
    public_path.write_bytes(public_pem)
    private_path.chmod(0o600)

    print(f"Clave privada (NO compartir, NO commitear): {private_path}")
    print(f"Clave pública (va en backend/license_public_key.pem):  {public_path}")


if __name__ == "__main__":
    main()
