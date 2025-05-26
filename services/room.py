from typing import List

from sqlalchemy.orm import Session

from models.room import Room as RoomModel
from schemas.room import RoomSchema


class RoomService:
    def create_room(
        self,
        room_name: str,
        host_client_id: str,
        db: Session,
    ) -> RoomSchema:
        # Create database room
        db_room = RoomModel(
            room_name=room_name,
            host_client_id=host_client_id,
        )

        db.add(db_room)
        db.commit()
        db.refresh(db_room)

        return RoomSchema.model_validate(db_room)

    def get_room(self, room_name: str, db: Session) -> RoomSchema | None:
        db_room = db.query(RoomModel).filter(RoomModel.room_name == room_name).first()
        if not db_room:
            return None
        return RoomSchema.model_validate(db_room)

    def get_all_rooms(self, db: Session) -> List[RoomSchema]:
        db_rooms = db.query(RoomModel).all()
        if not db_rooms:
            return []
        return [RoomSchema.model_validate(room) for room in db_rooms]

    def delete_room(self, room_name: str, db: Session) -> None:
        db_room = db.query(RoomModel).filter(RoomModel.room_name == room_name).first()
        if db_room:
            db.delete(db_room)
            db.commit()


# Create a singleton instance
room_service = RoomService()
