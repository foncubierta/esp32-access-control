import os
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from sqlmodel import Session, select

from database import create_db, engine
from models import AdminUser
from security import hash_password
import licensing

from routers import auth, users, credentials, doors, permissions, groups, logs, audit, node
from routers import license as license_router


def seed_admin():
    username = os.getenv("ADMIN_USERNAME", "admin")
    password = os.getenv("ADMIN_PASSWORD", "admin")
    with Session(engine) as session:
        existing = session.exec(select(AdminUser).where(AdminUser.username == username)).first()
        if existing:
            return
        admin = AdminUser(username=username, password_hash=hash_password(password))
        session.add(admin)
        session.commit()
        print(f"[seed] Created admin user '{username}' — change the password after first login.")


@asynccontextmanager
async def lifespan(app: FastAPI):
    create_db()
    seed_admin()
    with Session(engine) as session:
        licensing.enforce(session, actor="system")
    yield


app = FastAPI(title="ESP32 Access Control", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=os.getenv("CORS_ORIGINS", "*").split(","),
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth.router)
app.include_router(users.router)
app.include_router(credentials.router)
app.include_router(doors.router)
app.include_router(permissions.router)
app.include_router(groups.router)
app.include_router(logs.router)
app.include_router(audit.router)
app.include_router(node.router)
app.include_router(license_router.router)


@app.get("/api/health")
def health():
    return {"status": "ok"}
