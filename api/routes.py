import os
import platform
import subprocess
import threading
import time
import uuid
from typing import Any, Dict, List, Optional

from fastapi import APIRouter, Depends, HTTPException, Request
from pydantic import BaseModel
from sqlalchemy.orm import Session

from database import get_db
from schemas.room import RoomSchema
from services.room import room_service

router = APIRouter()


def get_ai_executable_command(exe_path: str) -> List[str]:
    return [exe_path]


@router.get("/")
async def root():
    return {"message": "Hello World"}


@router.post("/rooms")
async def create_room_endpoint(request: Request, db: Session = Depends(get_db)):
    try:
        data = await request.json()
        room_name = data.get("room_name")
        host_client_id = data.get("host_client_id")

        if not all([room_name, host_client_id]):
            raise HTTPException(status_code=400, detail="Missing required fields")

        room = room_service.create_room(
            room_name=room_name,
            host_client_id=host_client_id,
            db=db,
        )
        return room

    except Exception as e:
        print(f"Error in create_room_endpoint: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/rooms/{room_name}")
async def get_room_endpoint(
    room_name: str, db: Session = Depends(get_db)
) -> RoomSchema | None:
    try:
        room = room_service.get_room(room_name=room_name, db=db)
        if not room:
            return None
        return room

    except Exception as e:
        print(f"Error in get_room_endpoint: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/rooms")
async def list_rooms_endpoint(db: Session = Depends(get_db)) -> List[RoomSchema]:
    try:
        rooms = room_service.get_all_rooms(db)
        if not rooms:
            return []
        return [RoomSchema.model_validate(room) for room in rooms]

    except Exception as e:
        print(f"Error in list_rooms_endpoint: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))


@router.delete("/rooms/{room_name}")
async def delete_room_endpoint(
    room_name: str, db: Session = Depends(get_db)
) -> Dict[str, Any]:
    try:
        room_service.delete_room(
            room_name=room_name,
            db=db,
        )
        return {"status": "success"}

    except Exception as e:
        print(f"Error in delete_room_endpoint: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))


# ------------- Experiment - AI ------------- #


_ai_games = {}
_ai_games_lock = threading.Lock()
INACTIVITY_TIMEOUT = 300  # 5 minutes


def _cleanup_inactive_games():
    while True:
        time.sleep(60)
        now = time.time()
        with _ai_games_lock:
            to_remove = []
            for game_id, info in _ai_games.items():
                if now - info["last_active"] > INACTIVITY_TIMEOUT:
                    proc = info["proc"]
                    proc.kill()
                    proc.wait()
                    to_remove.append(game_id)
            for gid in to_remove:
                print(f"Cleaned up inactive game: {gid}")
                del _ai_games[gid]


cleanup_thread = threading.Thread(target=_cleanup_inactive_games, daemon=True)
cleanup_thread.start()


class AIMoveRequest(BaseModel):
    game_id: Optional[str] = None
    command: str


class AIMoveResponse(BaseModel):
    game_id: str
    move: str


@router.post("/ai/move", response_model=AIMoveResponse)
async def get_ai_move_endpoint(request: AIMoveRequest) -> AIMoveResponse:
    command = request.command
    game_id = request.game_id
    print(f"command: {command}, game_id: {game_id}")

    if not command:
        raise HTTPException(status_code=400, detail="Missing 'command' field")

    if not game_id:
        current_dir = os.path.dirname(os.path.abspath(__file__))
        exe_path = os.path.join(current_dir, "ai/pbrain-rapfi")
        ai_command = get_ai_executable_command(exe_path)

        proc = subprocess.Popen(
            ai_command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        lock = threading.Lock()
        game_id = str(uuid.uuid4())
        with _ai_games_lock:
            _ai_games[game_id] = {
                "proc": proc,
                "lock": lock,
                "last_active": time.time(),
            }
    else:
        with _ai_games_lock:
            if game_id not in _ai_games:
                raise HTTPException(status_code=404, detail="Game ID not found")
            proc_info = _ai_games[game_id]
            proc = proc_info["proc"]
            lock = proc_info["lock"]
            proc_info["last_active"] = time.time()

    with lock:
        try:
            if proc.stdin:
                proc.stdin.write(command + "\n")
                proc.stdin.flush()
            while True:
                if proc.stdout:
                    response = proc.stdout.readline().strip()
                    if not response or response.startswith("MESSAGE"):
                        continue
                    break
        except Exception as e:
            raise HTTPException(
                status_code=500, detail=f"AI process communication error: {e}"
            )

    return AIMoveResponse(game_id=game_id, move=response)
