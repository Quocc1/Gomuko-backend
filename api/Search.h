#pragma once
#include "Evaluator.h"
#include <climits>

class HashTable;

enum WinState { State_Unknown, State_Win, State_Lose, State_Draw };

enum NodeType { NonPV = 0, PV = 1 };

enum GenLevel { InNone, InLine, InArea, InFullBoard };

struct MoveList {
    static const int MAX_MOVES = 128;
    enum Phase : UInt8 { HashMove, GenAllMoves, AllMoves };

    typedef vector<Move>::iterator MoveIterator;

    vector<Move> moves;
    Pos hashMove;
    Phase phase;
    size_t n;

    MoveList() {
        moves.reserve(MAX_MOVES);
        init();
    }
    inline void init(const Pos & hashMove_ = NullPos) {
        moves.clear();
        phase = HashMove;
        hashMove = hashMove_;
        n = 0;
    }
    inline void init_GenAllMoves() {
        moves.clear();
        phase = GenAllMoves;
        n = 0;
    }
    inline void addMove(Pos p, int value) { moves.emplace_back(p, value); }
    inline size_t moveCount() { return moves.size(); }
    inline MoveIterator begin() { return moves.begin(); }
    inline MoveIterator end() { return moves.end(); }
};

struct Line {
    list<Pos> moves;

    void clear() { moves.clear(); }
    void pushMove(Pos pos) { moves.push_back(pos); }
    friend ostream & operator<<(ostream & out, const Line & line) {
        for (Pos p : line.moves) { out << PosStr(p); }
        return out;
    }
    string YXPrint(UInt8 boardSize) {
        std::ostringstream s;
        for (Pos p : moves) { s << YXPos(p, boardSize); }
        return s.str();
    }
};

//Search Preset Config
#define Internal_Iterative_Deepening
#define Futility_Pruning
#define Razoring
///#define Late_Move_Reduction
///#define Singular_Extension

#define Win_Check_FLEX3_2X

#define Hash_Probe
#define Hash_Record

#define VCF_Leaf
///#define VCF_Branch_Limit

class AI : public Evaluator {
private:
    static const long TIME_RESERVED = 40;            // ms
    static const long TIME_RESERVED_PER_MOVE = 200;  // ms
    static const int MATCH_SPARE = 23;         // how much is time spared for the rest of game
    static const int MATCH_SPARE_MIN = 7;      // min time spared for the rest of game
    static const int MATCH_SPARE_MAX = 40;     // max time spared for the rest of game
    static const int TIMEOUT_PREVENT = 45;     // alphabeta become slow when depth increases
    static const int TIMEOUT_PREVENT_MIN = 70; // (TIMEOUT_PREVENT_MIN / 100.0)�ٷ���
    static const int BM_CHANGE_MIN = 3;        // if bestmove changes more than this, increase time
    static const int BM_CHANGE_MIN_DEPTH = 7;  // max bestmove changes taken into account
    static const int BM_STABLE_MIN = 3;        // if bestmove remains same more than this, decrease time
    static const int TIME_INCRESE_PERCENTAGE = 105;  // ʱ�����ӵİٷ���
    static const int TIME_DECREASE_PERCENTAGE = 90;  // ʱ����ٵİٷ���
    static const int TURNTIME_MIN_DIVISION = 3;      // min time for this turn

    static const int MAX_SEARCH_DEPTH = 64;    // ����ȫ��������������
    static const int MAX_PLY = 150;            // �����ܼ����������
    static const int MAX_WINNING_CHECK_BRANCH = 50;  // ��ľ��������ķ�֧

    static const int EXTENSION_NUM_BASE = 20;  // ����Ļ�׼��,Խ������Խ��
    float depthReductionBase = 1.f / logf((float)EXTENSION_NUM_BASE);

    static const int IID_MIN_DEPTH = 8;
    int IIDMinDepth = IID_MIN_DEPTH;

    static const int FUTILITY_MAX_DEPTH = 4;
    int FutilityDepth = FUTILITY_MAX_DEPTH;
    int FutilityMargin[4] = { 100, 160, 200, 250 };

    static const int RAZORING_MAX_DEPTH = 4;
    int RazoringDepth = RAZORING_MAX_DEPTH;
    int RazoringMargin[4] = { 150, 200, 250, 300 };

    float SEBetaMargin = 3.0f;

    // VCF ����
    static const int MAX_VCF_BRANCH = 10;
    static const int MAX_VCF_PLY = 36;

    // ���������������پ���
    static const int CONTINUES_NEIGHBOR = 2;       // ���ŷ�(ͬ��)������
    static const int CONTINUES_DISTANCE = 4;       // ���ŷ�(ͬ��)������
    static const int CONTINUES_DISTANCE_LARGE = 6; // ���ŷ�(����)������

    bool useOpeningBook = true;   // �Ƿ�ʹ�ÿ��ֿ�
    bool reloadConfig = false;    // �Ƿ���ÿ��˼��ǰ��������config
    int maxSearchDepth = MAX_SEARCH_DEPTH;

    Piece aiPiece;
    Piece attackerPiece;
    HashTable * hashTable = nullptr;
    MoveList moveLists[MAX_PLY];

    bool isPvExact[MAX_PLY] = { true, false };
    int rawStaticEval[MAX_PLY];
    int minEvalPly = 0;
    int singularExtensionPly = -1;   // �����������������Ĳ���
    Pos excludedMove = NullPos;
    Pattern4 lastSelfP4, lastOppoP4;

    long startTime;
    bool terminateAI;
    int maxPlyReached;
    int VCFMaxPly, VCTMaxPly;
    int BestMoveChangeCount;

    // ����ͳ�Ʊ���
    int node, nodeExpended;

    /////////////////////////////////////////////////////////////
    // ʱ����� Time Management

    inline long timeUsed() { return getTime() - startTime; }
    inline long timeLeft() {
#ifdef _DEBUG
        return INF;
#else
        return info.time_left;
#endif
    }
    inline long timeForTurn() {
        double timePercentage = (double)board->getMoveLeftCount() / board->maxCells();
        return MIN(info.timeout_turn, 
            timeLeft() / (MAX((long)round(MATCH_SPARE * timePercentage), MATCH_SPARE_MIN))) - TIME_RESERVED;
    }
    inline long timeForTurnMax() {
        return MIN(info.timeout_turn, timeLeft() / MATCH_SPARE_MIN) - TIME_RESERVED;
    }

    /////////////////////////////////////////////////////////////

    // ��������
    Pos fullSearch();
    Move alphabeta_root(int depth, int alpha, int beta);
    template <NodeType NT> int alphabeta(float depth, int alpha, int beta, bool cutNode);
    template <bool Root = true> int quickVCFSearch();

    /////////////////////////////////////////////////////////////

    // �ŷ�ѭ��,���ݷ���ȷ���Ƿ�Ӧ��������
    bool moveNext(MoveList & moveList, Pos & pos);
    
    // �ŷ�����
    WinState genMove_Root(MoveList & moveList);
    void genMoves(MoveList & moveList);
    void genMoves_defence(MoveList & moveList);
    void genMoves_VCF(MoveList & moveList);
    void genContinueMoves_VCF(MoveList & moveList, const short * range, int n);

    /////////////////////////////////////////////////////////////

    // �������֧��
    inline int getMaxBranch(int ply) {
        return MAX(64 - 2 * ply, 25);
    }
    // ����������
    inline void updateMaxPlyReached(int ply) {
        if (ply > maxPlyReached) maxPlyReached = ply;
    }
    // ���ݵ�ǰ��������ȵݼ���
    float getDepthReduction();
    // ���û�������ȡPV·��
    void fetchPVLineInTT(Line & line, Pos firstMove, int maxDepth = MAX_PLY);
    // �����ж�˫�����Ƿ��ʤ
    int quickDefenceCheck();

public:
    static const int INF = INT_MAX - 1;
    static const int WIN_MAX = 30000;
    static const int WIN_MIN = 29000;
    struct Info {
        long timeout_turn = 5 * 1000;   // millseconds
        long timeout_match = 100000 * 1000;
        long time_left = timeout_match;
        long max_memory = LONG_MAX;
        bool exact5 = false;
        bool renju = false;

        void setMaxMemory(long maxMemory) { max_memory = maxMemory ? maxMemory : LONG_MAX; }
    } info;

    AI(Board * board);
    ~AI();

    void stopThinking() { terminateAI = true; }
    void clearHash();
    void setMaxDepth(int depth);
    Pos turnMove();

    inline int evaluate();
    inline int rawEvaluate();
    int quickWinCheck();

    void newGame();

    // Read Config File
    void tryReadConfig(string path);
    bool shouldReloadConfig() { return reloadConfig; }
};