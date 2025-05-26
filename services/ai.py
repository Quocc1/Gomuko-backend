import random
import time
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

from models.game import Position

# Pattern types
WIN = 0
FLEX4 = 1  # Live four
BLOCK4 = 2  # Blocked four
FLEX3 = 3  # Live three
BLOCK3 = 4  # Blocked three
FLEX2 = 5  # Live two
BLOCK2 = 6  # Blocked two
FLEX1 = 7  # Live one

# Hash flags
HASH_EXACT = 0
HASH_ALPHA = 1
HASH_BETA = 2


@dataclass
class HashEntry:
    key: int = 0
    value: int = 0
    depth: int = 0
    flag: int = 0


@dataclass
class PVEntry:
    key: int = 0
    best_move: Tuple[int, int] = (0, 0)


@dataclass
class Cell:
    piece: int = 0  # 0=empty, 1=player1, 2=player2
    is_candidate: bool = False
    patterns: Dict[int, List[int]] = None

    def __post_init__(self):
        if self.patterns is None:
            self.patterns = {1: [0, 0, 0, 0], 2: [0, 0, 0, 0]}


class GomokuAI:
    def __init__(
        self, board_size: int = 15, game_rules: Optional[Dict[str, bool]] = None
    ):
        self.board_size = board_size
        self.extended_size = board_size + 8  # Add padding
        self.board = [
            [Cell() for _ in range(self.extended_size)]
            for _ in range(self.extended_size)
        ]

        # Game rules
        self.game_rules = game_rules or {}
        self.exactly_five_rule = self.game_rules.get("exactlyFiveRule", False)
        self.no_blocked_wins_rule = self.game_rules.get("noBlockedWinsRule", False)

        # Hash tables
        self.hash_size = 1 << 20  # 1M entries
        self.hash_table = [HashEntry() for _ in range(self.hash_size)]
        self.pv_table = [PVEntry() for _ in range(self.hash_size)]

        # Search parameters
        self.max_depth = 20
        self.min_depth = 4
        self.max_moves = 20
        self.timeout = 5000  # 5 seconds

        # Game state
        self.current_player = 1
        self.move_history = []
        self.zobrist_key = 0
        self.step_count = 0

        # Search statistics
        self.nodes_searched = 0
        self.hash_hits = 0
        self.start_time = 0
        self.stop_search = False

        # Pattern evaluation values (adjusted for new rules)
        self.pattern_values = {
            WIN: 10000,
            FLEX4: 4320,
            BLOCK4: 720
            if not self.no_blocked_wins_rule
            else 360,  # Reduced value if blocked wins don't count
            FLEX3: 720,
            BLOCK3: 120,
            FLEX2: 120,
            BLOCK2: 20,
            FLEX1: 20,
        }

        # Initialize Zobrist keys for hashing
        self.zobrist_keys = self._init_zobrist()

        # Initialize pattern tables
        self._init_patterns()

    def _init_zobrist(self):
        """Initialize Zobrist hash keys"""
        random.seed(12345)  # Fixed seed for consistency
        keys = {}
        for i in range(self.extended_size):
            for j in range(self.extended_size):
                keys[(i, j, 1)] = random.getrandbits(64)
                keys[(i, j, 2)] = random.getrandbits(64)
        return keys

    def _init_patterns(self):
        """Initialize pattern recognition tables"""
        # Simplified pattern initialization
        # In a full implementation, this would be much more comprehensive
        pass

    def get_time_elapsed(self) -> int:
        """Get elapsed search time in milliseconds"""
        return int((time.time() - self.start_time) * 1000)

    def should_stop_search(self) -> bool:
        """Check if search should be stopped due to time limit"""
        return self.get_time_elapsed() >= self.timeout

    def make_move(self, row: int, col: int, player: int):
        """Make a move on the board"""
        # Convert to internal coordinates (add padding)
        internal_row, internal_col = row + 4, col + 4

        self.board[internal_row][internal_col].piece = player
        self.move_history.append((internal_row, internal_col, player))

        # Update Zobrist key
        if (internal_row, internal_col, player) in self.zobrist_keys:
            self.zobrist_key ^= self.zobrist_keys[(internal_row, internal_col, player)]

        # Update candidate positions around the move
        self._update_candidates(internal_row, internal_col)

        self.step_count += 1

    def undo_move(self):
        """Undo the last move"""
        if not self.move_history:
            return

        row, col, player = self.move_history.pop()
        self.board[row][col].piece = 0

        # Update Zobrist key
        if (row, col, player) in self.zobrist_keys:
            self.zobrist_key ^= self.zobrist_keys[(row, col, player)]

        self.step_count -= 1

    def _update_candidates(self, row: int, col: int):
        """Update candidate positions around a move"""
        for dr in range(-2, 3):
            for dc in range(-2, 3):
                new_row, new_col = row + dr, col + dc
                if (
                    4 <= new_row < self.extended_size - 4
                    and 4 <= new_col < self.extended_size - 4
                ):
                    if self.board[new_row][new_col].piece == 0:
                        self.board[new_row][new_col].is_candidate = True

    def _count_consecutive_with_ends(
        self, row: int, col: int, dx: int, dy: int, player: int
    ) -> Tuple[int, int]:
        """
        Count consecutive pieces in a direction and return (count, blocked_ends)

        Returns:
            tuple: (consecutive_count, blocked_ends_count)
                   blocked_ends: 0=both ends open, 1=one end blocked, 2=both ends blocked
        """
        count = 1  # Count the piece at (row, col)
        blocked_ends = 0

        # Check positive direction
        x, y = row + dx, col + dy
        while (
            4 <= x < self.extended_size - 4
            and 4 <= y < self.extended_size - 4
            and self.board[x][y].piece == player
        ):
            count += 1
            x, y = x + dx, y + dy

        # Check if positive end is blocked
        if (
            x < 4
            or x >= self.extended_size - 4
            or y < 4
            or y >= self.extended_size - 4
            or self.board[x][y].piece == (3 - player)
        ):
            blocked_ends += 1

        # Check negative direction
        x, y = row - dx, col - dy
        while (
            4 <= x < self.extended_size - 4
            and 4 <= y < self.extended_size - 4
            and self.board[x][y].piece == player
        ):
            count += 1
            x, y = x - dx, y - dy

        # Check if negative end is blocked
        if (
            x < 4
            or x >= self.extended_size - 4
            or y < 4
            or y >= self.extended_size - 4
            or self.board[x][y].piece == (3 - player)
        ):
            blocked_ends += 1

        return count, blocked_ends

    def check_win(self, player: int) -> bool:
        """Check if a player has won according to game rules"""
        directions = [(0, 1), (1, 0), (1, 1), (1, -1)]

        for i in range(4, self.extended_size - 4):
            for j in range(4, self.extended_size - 4):
                if self.board[i][j].piece != player:
                    continue

                for dx, dy in directions:
                    count, blocked_ends = self._count_consecutive_with_ends(
                        i, j, dx, dy, player
                    )

                    # Apply exactly five rule
                    if self.exactly_five_rule:
                        if count != 5:
                            continue
                    else:
                        if count < 5:
                            continue

                    # Apply no blocked wins rule
                    if self.no_blocked_wins_rule:
                        if blocked_ends >= 2:  # Both ends blocked
                            continue

                    # If we reach here, it's a valid win
                    return True

        return False

    def _is_valid_win_line(self, count: int, blocked_ends: int) -> bool:
        """Check if a line configuration counts as a win according to game rules"""
        # Apply exactly five rule
        if self.exactly_five_rule and count != 5:
            return False
        elif not self.exactly_five_rule and count < 5:
            return False

        # Apply no blocked wins rule
        if self.no_blocked_wins_rule and blocked_ends >= 2:
            return False

        return True

    def evaluate_position(self) -> int:
        """Evaluate the current position"""
        if self.check_win(3 - self.current_player):  # Opponent won
            return -10000

        # Simplified evaluation - count patterns for both players
        my_score = self._count_patterns(self.current_player)
        opp_score = self._count_patterns(3 - self.current_player)

        # Player to move gets slight advantage
        return int(my_score * 1.2 - opp_score)

    def _count_patterns(self, player: int) -> int:
        """Count and score patterns for a player"""
        score = 0
        # Simplified pattern counting
        # In a real implementation, this would use the pattern tables

        directions = [(0, 1), (1, 0), (1, 1), (1, -1)]

        for i in range(4, self.extended_size - 4):
            for j in range(4, self.extended_size - 4):
                if self.board[i][j].piece == 0 and self.board[i][j].is_candidate:
                    # Evaluate what patterns this empty position could create
                    move_value = self._evaluate_move(i, j, player)
                    if move_value > 0:
                        score += move_value

        return score

    def _evaluate_move(self, row: int, col: int, player: int) -> int:
        """Evaluate the value of placing a piece at this position"""
        directions = [(0, 1), (1, 0), (1, 1), (1, -1)]
        total_score = 0

        for dx, dy in directions:
            # Simulate placing the piece
            self.board[row][col].piece = player
            count, blocked_ends = self._count_consecutive_with_ends(
                row, col, dx, dy, player
            )
            self.board[row][col].piece = 0  # Remove the simulated piece

            # Score based on count and blocked ends, considering game rules
            pattern_score = self._score_pattern(count, blocked_ends)
            total_score += pattern_score

        return total_score

    def _score_pattern(self, count: int, blocked_ends: int) -> int:
        """Score a pattern based on count and blocked ends, considering game rules"""
        # Check if this would be a winning pattern
        if self._is_valid_win_line(count, blocked_ends):
            return 10000

        # Adjust scoring based on game rules
        if count >= 5:
            # If exactly five rule is enabled, 6+ stones might not be useful
            if self.exactly_five_rule and count > 5:
                return 50  # Lower value for over-lines
            elif not self.exactly_five_rule:
                return 10000  # Normal win
        elif count == 4:
            if blocked_ends == 0:
                return 4320  # Live four
            elif blocked_ends == 1:
                # Blocked four is less valuable if blocked wins don't count
                return 720 if not self.no_blocked_wins_rule else 360
            else:
                return 20  # Doubly blocked four
        elif count == 3:
            if blocked_ends == 0:
                return 720  # Live three
            elif blocked_ends == 1:
                return 120  # Blocked three
            else:
                return 10  # Doubly blocked three
        elif count == 2:
            if blocked_ends == 0:
                return 120  # Live two
            elif blocked_ends == 1:
                return 20  # Blocked two
            else:
                return 5  # Doubly blocked two
        elif count == 1:
            return 20 if blocked_ends == 0 else 5

        return 0

    def generate_moves(self) -> List[Tuple[int, int]]:
        """Generate candidate moves"""
        moves = []
        candidates = []

        # Collect all candidate positions with their values
        for i in range(4, self.extended_size - 4):
            for j in range(4, self.extended_size - 4):
                if self.board[i][j].piece == 0 and self.board[i][j].is_candidate:
                    # Evaluate for current player and opponent
                    my_value = self._evaluate_move(i, j, self.current_player)
                    opp_value = self._evaluate_move(i, j, 3 - self.current_player)

                    # Prioritize blocking opponent's threats
                    total_value = my_value + opp_value * 1.1

                    if total_value > 0:
                        candidates.append(((i, j), total_value))

        # Sort by value (descending)
        candidates.sort(key=lambda x: x[1], reverse=True)

        # Return top moves
        for (i, j), _ in candidates[: self.max_moves]:
            moves.append((i, j))

        return moves

    def alpha_beta(self, depth: int, alpha: int, beta: int) -> int:
        """Alpha-beta search"""
        self.nodes_searched += 1

        if self.nodes_searched % 1000 == 0:
            if self.should_stop_search():
                self.stop_search = True
                return alpha

        if self.check_win(3 - self.current_player):
            return -10000

        if depth <= 0:
            return self.evaluate_position()

        moves = self.generate_moves()
        if not moves:
            return self.evaluate_position()

        best_value = -10001

        for row, col in moves:
            if self.stop_search:
                break

            self.make_move(
                row - 4, col - 4, self.current_player
            )  # Convert back to board coords
            self.current_player = 3 - self.current_player

            value = -self.alpha_beta(depth - 1, -beta, -alpha)

            self.current_player = 3 - self.current_player
            self.undo_move()

            if value > best_value:
                best_value = value

            if value >= beta:
                return value

            if value > alpha:
                alpha = value

        return best_value

    def search_best_move(self) -> Tuple[int, int]:
        """Main search function with iterative deepening"""
        self.start_time = time.time()
        self.nodes_searched = 0
        self.hash_hits = 0
        self.stop_search = False

        best_move = None

        # First move: center
        if self.step_count == 0:
            return (self.board_size // 2, self.board_size // 2)

        # Early moves: random around center with some variation
        if self.step_count <= 2:
            center = self.board_size // 2
            attempts = 0
            while attempts < 100:
                offset = random.randint(-2, 2)
                row = (
                    center
                    + random.randint(-1, 1)
                    + (offset if self.step_count == 2 else 0)
                )
                col = (
                    center
                    + random.randint(-1, 1)
                    + (offset if self.step_count == 2 else 0)
                )

                if (
                    0 <= row < self.board_size
                    and 0 <= col < self.board_size
                    and self.board[row + 4][col + 4].piece == 0
                ):
                    return (row, col)
                attempts += 1

        # Iterative deepening search
        moves = self.generate_moves()
        if not moves:
            return (self.board_size // 2, self.board_size // 2)

        best_move = moves[0]  # Fallback

        for depth in range(self.min_depth, self.max_depth + 1, 2):
            if self.stop_search:
                break

            best_value = -10001
            current_best = None

            for row, col in moves:
                if self.stop_search:
                    break

                self.make_move(row - 4, col - 4, self.current_player)
                self.current_player = 3 - self.current_player

                value = -self.alpha_beta(depth - 1, -10000, 10000)

                self.current_player = 3 - self.current_player
                self.undo_move()

                if value > best_value:
                    best_value = value
                    current_best = (row - 4, col - 4)  # Convert to board coordinates

                if best_value >= 10000:  # Found winning move
                    self.stop_search = True
                    break

            if current_best:
                best_move = current_best

            # Stop if we've found a definitive result or running out of time
            if (
                self.stop_search
                or best_value >= 9000
                or self.get_time_elapsed() * 3 > self.timeout
            ):
                break

        return best_move


def get_ai_move(
    board: List[List[int]],
    current_player: int,
    game_rules: Optional[Dict[str, bool]] = None,
) -> Position:
    """
    Get AI move for Gomoku game

    Args:
        board: 2D list representing the game board (0=empty, 1=player1, 2=player2)
        current_player: Current player (1 or 2)
        strategy: AI strategy (currently only supports "genius")
        game_rules: Optional game rules dictionary with keys:
                   - exactlyFiveRule: Only exactly 5 stones in a row win (not 6+)
                   - noBlockedWinsRule: Lines must have at least one open end to win

    Returns:
        Position object with row and col of the best move
    """

    print(f"Game Rules: {game_rules}")

    # Initialize AI with game rules
    ai = GomokuAI(len(board), game_rules)
    ai.current_player = current_player

    # Set up the board state
    for i in range(len(board)):
        for j in range(len(board[0])):
            if board[i][j] != 0:
                ai.make_move(i, j, board[i][j])

    # Get best move
    row, col = ai.search_best_move()

    return Position(row=row, col=col)
