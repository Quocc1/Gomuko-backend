from typing import Any, Dict, List

from fastapi import APIRouter, Depends, HTTPException, Request
from sqlalchemy.orm import Session

from database import get_db
from schemas.room import RoomSchema
from services.ai import get_ai_move
from services.room import room_service

router = APIRouter()

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


@router.post("/ai/move")
async def get_ai_move_endpoint(request: Request) -> Dict[str, Any]:
    try:
        data = await request.json()
        board = data.get("board")
        current_player = data.get("current_player")
        game_rules = data.get(
            "game_rules", {"exactlyFiveRule": True, "noBlockedWinsRule": False}
        )

        if not board or current_player is None:
            raise HTTPException(status_code=400, detail="Missing required fields")

        # Get AI move
        move = get_ai_move(
            board=board,
            current_player=current_player,
            game_rules=game_rules,
        )

        return {
            "move": {
                "row": move.row,
                "col": move.col,
            }
        }

    except Exception as e:
        print(f"Error in get_ai_move_endpoint: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))
