from pydantic import BaseModel


class RoomSchema(BaseModel):
    id: int
    room_name: str
    host_client_id: str

    class Config:
        from_attributes = True
