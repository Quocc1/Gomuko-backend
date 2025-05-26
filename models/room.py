from sqlalchemy import Column, String, Integer, Sequence

from database import Base


class Room(Base):
    __tablename__ = "rooms"

    id = Column(Integer, Sequence('room_id_seq'), primary_key=True, index=True)
    room_name = Column(String, nullable=False)
    host_client_id = Column(String, nullable=False)
