#!/usr/bin/env python3
"""Issues a signed license token for a customer deployment.

    python3 generate_license.py --max-doors 3 --customer "Acme S.L." \\
        [--expires-days 365] [--private-key keys/private_key.pem]

Prints the token to stdout — paste it into the "Licencia" page of the
customer's admin panel. Without --expires-days the license never expires.
"""
import argparse
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path

import jwt

ALGORITHM = "EdDSA"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--max-doors", type=int, required=True, help="Número de puertas/nodos que permite la licencia")
    parser.add_argument("--customer", required=True, help="Nombre del cliente (solo informativo, se muestra en el panel)")
    parser.add_argument("--expires-days", type=int, default=None, help="Días de validez desde hoy (omite para que no caduque)")
    parser.add_argument(
        "--private-key",
        default=str(Path(__file__).parent / "keys" / "private_key.pem"),
        help="Ruta a la clave privada (default: ./keys/private_key.pem)",
    )
    args = parser.parse_args()

    if args.max_doors < 0:
        sys.exit("--max-doors no puede ser negativo")

    private_key_path = Path(args.private_key)
    if not private_key_path.exists():
        sys.exit(f"No se encuentra la clave privada en {private_key_path} — ejecuta generate_keypair.py primero")
    private_key = private_key_path.read_text()

    now = datetime.now(timezone.utc)
    payload = {
        "max_doors": args.max_doors,
        "customer": args.customer,
        "iat": now,
    }
    if args.expires_days is not None:
        payload["exp"] = now + timedelta(days=args.expires_days)

    token = jwt.encode(payload, private_key, algorithm=ALGORITHM)
    print(token)


if __name__ == "__main__":
    main()
