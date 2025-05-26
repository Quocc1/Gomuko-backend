from typing import List
from ..models.game import Position, PLAYER_PIECE


def check_win(
    board: List[List[int]], player_piece: int, r_move: int, c_move: int
) -> bool:
    if player_piece == PLAYER_PIECE["NONE"]:
        return False

    board_size = len(board)

    # Check horizontal
    count = 0
    for c in range(board_size):
        if board[r_move][c] == player_piece:
            count += 1
            if count == 5:
                return True
        else:
            count = 0

    # Check vertical
    count = 0
    for r in range(board_size):
        if board[r][c_move] == player_piece:
            count += 1
            if count == 5:
                return True
        else:
            count = 0

    # Check positive diagonal
    count = 0
    for i in range(-4, 5):
        r, c = r_move + i, c_move + i
        if 0 <= r < board_size and 0 <= c < board_size and board[r][c] == player_piece:
            count += 1
            if count == 5:
                return True
        elif count < 5:  # only reset if we were in a sequence and it broke
            if not (
                0 <= r < board_size
                and 0 <= c < board_size
                and board[r][c] == player_piece
            ):
                count = 0  # reset if outside board or not player_piece, and streak was broken
        else:  # count >=5
            pass

    # Check negative diagonal
    count = 0
    for i in range(-4, 5):
        r, c = r_move + i, c_move - i
        if 0 <= r < board_size and 0 <= c < board_size and board[r][c] == player_piece:
            count += 1
            if count == 5:
                return True
        elif count < 5:
            if not (
                0 <= r < board_size
                and 0 <= c < board_size
                and board[r][c] == player_piece
            ):
                count = 0
        else:  # count >=5
            pass
    return False


def check_draw(board: List[List[int]]) -> bool:
    board_size = len(board)
    for r in range(board_size):
        for c in range(board_size):
            if board[r][c] == PLAYER_PIECE["NONE"]:
                return False
    return True


def get_valid_moves(board: List[List[int]]) -> List[Position]:
    moves = []
    board_size = len(board)
    visited = set()

    # Look for stones and their surrounding positions
    for row in range(board_size):
        for col in range(board_size):
            if board[row][col] != PLAYER_PIECE["NONE"]:
                # Check surrounding positions
                for dr in range(-2, 3):
                    for dc in range(-2, 3):
                        new_row = row + dr
                        new_col = col + dc
                        key = f"{new_row},{new_col}"

                        if (
                            0 <= new_row < board_size
                            and 0 <= new_col < board_size
                            and board[new_row][new_col] == PLAYER_PIECE["NONE"]
                            and key not in visited
                        ):
                            moves.append(Position(row=new_row, col=new_col))
                            visited.add(key)

    # If no moves found (empty board), return center position
    if not moves:
        center = board_size // 2
        return [Position(row=center, col=center)]

    return moves
