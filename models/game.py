from typing import List, Literal, Optional

from pydantic import BaseModel

# Constants
PLAYER_PIECE = {
    "NONE": 0,
    "BLACK": 1,  # Typically player 1 / host
    "WHITE": 2,  # Typically player 2 / guest
}
DEFAULT_BOARD_SIZE = 15
MAX_PLAYERS = 2


class Position(BaseModel):
    row: int
    col: int

    def __hash__(self) -> int:
        return hash((self.row, self.col))

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Position):
            return NotImplemented
        return self.row == other.row and self.col == other.col


class Player(BaseModel):
    client_id: str
    username: str
    player_piece: int  # PLAYER_PIECE["BLACK"] or PLAYER_PIECE["WHITE"]


class Spectator(BaseModel):
    client_id: str
    username: str


class RoomCreateRequest(BaseModel):
    room_name: str
    host_client_id: str
    host_username: Optional[str] = "Host"


class JoinRoomRequest(BaseModel):
    client_id: str
    username: Optional[str] = "Guest"


class RoomMetadata(BaseModel):
    id: str
    name: str
    player_count: int
    spectator_count: int
    status: Literal["waiting", "in_progress", "finished"]


class GameConfig(BaseModel):
    board_size: int = DEFAULT_BOARD_SIZE
    exactly_five_rule: bool = True
    no_blocked_wins_rule: bool = False


class MakeMoveRequest(BaseModel):
    client_id: str
    row: int
    col: int


class BoardStateAI(BaseModel):
    board: List[List[int]]
    current_player: int
    strategy: str
    game_rules: dict
