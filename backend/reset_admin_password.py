#!/usr/bin/env python3
"""Reset (or create) an admin login directly against the database — for
when you're locked out and don't have a working password to change it
through the panel/API (see POST /api/auth/change-password for that normal
path, which needs the current password).

Run this on the machine hosting the backend, from the backend/ directory,
using the same virtualenv the service runs with:

    cd backend
    .venv/bin/python3 reset_admin_password.py <nueva_contraseña>

Or activate the venv first and just `python3 reset_admin_password.py ...`.
"""
import argparse
import sys

from dotenv import load_dotenv

load_dotenv()

from sqlalchemy.exc import OperationalError  # noqa: E402
from sqlmodel import Session, select  # noqa: E402

from database import engine  # noqa: E402
from models import AdminUser  # noqa: E402
from security import hash_password  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("new_password", help="Nueva contraseña (mínimo 8 caracteres)")
    parser.add_argument("--username", default="admin", help="Usuario a resetear (por defecto: admin)")
    args = parser.parse_args()

    if len(args.new_password) < 8:
        print("La contraseña debe tener al menos 8 caracteres.", file=sys.stderr)
        sys.exit(1)

    try:
        with Session(engine) as session:
            admin = session.exec(select(AdminUser).where(AdminUser.username == args.username)).first()
            if admin:
                admin.password_hash = hash_password(args.new_password)
                session.add(admin)
                session.commit()
                print(f"Contraseña de '{args.username}' actualizada.")
            else:
                admin = AdminUser(username=args.username, password_hash=hash_password(args.new_password))
                session.add(admin)
                session.commit()
                print(f"No existía el usuario '{args.username}' — creado con la contraseña indicada.")
    except OperationalError:
        print(
            "No se pudo leer la base de datos — ¿has arrancado el backend al menos una vez? "
            "(las tablas se crean en el primer arranque).",
            file=sys.stderr,
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
