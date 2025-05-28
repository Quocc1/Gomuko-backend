#include "Search.h"
#include "HashTable.h"
#include <fstream>
#include <cstring>

AI::AI(Board * board) : Evaluator(board) {
    hashTable = new HashTable();
}

AI::~AI() {
    delete hashTable;
}

void AI::newGame() {
    Evaluator::newGame();
    clearHash();
}

void AI::clearHash() {
    hashTable->clearHash();
}

void AI::setMaxDepth(int depth) {
    maxSearchDepth = MAX(MIN(depth, 255), 2);
}

Pos AI::turnMove() {
    startTime = getTime();
    terminateAI = false;

    if (board->getMoveCount() == 0)
        return POS(board->centerPos(), board->centerPos());
    else if (board->getMoveCount() == 1) {
        Pos p = board->getLastMove();
        if (CoordX(p) == -BOARD_BOUNDARY) {
            return POS(board->centerPos(), board->centerPos());
        }
        if (!board->isNearBoard(p, 5)) {
            Pos b;
            std::uniform_int_distribution<> dis(-1, 1);
            do {
                b = POS((CoordX(p)) + dis(random), CoordY(p) + dis(random));
            } while (!board->isEmpty(b));
            return b;
        } else if (board->isNearBoard(p, 1)) {
            expendCand(p, 3, 5);
        } else if (board->isNearBoard(p, 2)) {
            expendCand(p, 3, 4);
        }
    } else if (board->getMoveLeftCount() == 0)
        return NullPos;

    Pos best;
#ifndef VERSION_YIXIN_BOARD
    if (useOpeningBook) {
        best = databaseMove();
        if (board->isEmpty(best))
            return best;
    }
#endif

    node = nodeExpended = 0;
    ply = 0;
    maxPlyReached = 0;
    clearLose();

#ifdef VERSION_YIXIN_BOARD
    clearHash();
    MESSAGEL("Expect Time: " << timeForTurn() << "ms");
#else
    if (reloadConfig)
        clearHash();
#endif
    aiPiece = SELF;
    hashTable->newSearch();

    best = fullSearch();

    long time = timeUsed();
#ifdef VERSION_YIXIN_BOARD
    MESSAGEL("Node: " << node << " Speed: " << node / (time + 1) << "K");
    MESSAGEL("Time: " << time << " ms");
#else
    MESSAGEL("�ڵ���: " << node << " NPS: " << node / (time + 1) << "K");
    MESSAGEL("�ڵ�չ����: " << nodeExpended << " NPS: " << nodeExpended / (time + 1) << "K");
    MESSAGEL("ƽ����֧: " << (nodeExpended == 0 ? 0 : double(node) / nodeExpended));
    MESSAGEL("�ܺ�ʱ: " << time << " ms");
#endif
    return best;
}

Pos AI::fullSearch() {
    long lastDepthTime, turnTime;
    Move bestMove, move;
    int lastValue = -INF;
    int lastNode, lastNodeExpended;
    int searchDepth;
    int PVStableCount = 0;
    bool shouldBreak;

    turnTime = timeForTurn();

    moveLists[0].init_GenAllMoves();
    for (searchDepth = 2; searchDepth <= maxSearchDepth; searchDepth++) {
        lastDepthTime = getTime();
        lastNode = node;
        lastNodeExpended = nodeExpended;
        BestMoveChangeCount = 0;

        move = alphabeta_root(searchDepth, -INF, INF);
        if (!board->isEmpty(move.pos)) break;
        else if (move.value == -INF)  // ���ʱ��֪����һ����������ֵ������һ���Ĵ���
            move.value = bestMove.value; 
        bestMove = move;

        if (nodeExpended != lastNodeExpended) {
            if (BestMoveChangeCount == 0) {
                if (++PVStableCount >= BM_STABLE_MIN)
                    turnTime = MAX(turnTime * TIME_DECREASE_PERCENTAGE / 100, timeForTurn() / TURNTIME_MIN_DIVISION);
            } else {
                PVStableCount = 0;
                if (searchDepth >= BM_CHANGE_MIN_DEPTH)
                    turnTime = MIN(turnTime * TIME_INCRESE_PERCENTAGE / 100, timeForTurnMax());
            }
        }

        // ��ǰ�˳�|��ʱ�˳�|Ӯ�����˳�|ƽ���˳�
        shouldBreak = (timeLeft() / MATCH_SPARE_MAX < info.timeout_turn || timeUsed() > turnTime * TIMEOUT_PREVENT_MIN / 100) && (turnTime * 10 <= (getTime() - lastDepthTime) * TIMEOUT_PREVENT)
            || terminateAI
            || bestMove.value >= WIN_MIN || bestMove.value <= -WIN_MIN;

        // �������ڵ�(����ȡ�û���)���������Ϣ
        if (nodeExpended != lastNodeExpended || shouldBreak)
        #ifdef VERSION_YIXIN_BOARD
            MESSAGEL("Depth:" << searchDepth << "-" << maxPlyReached << " Val:" << bestMove.value << " Time:" << getTime() - lastDepthTime << "ms Node:" << (node - lastNode) / 1000000 << "M " << YXPos(bestMove.pos, board->size()));
    #else
            MESSAGEL("���: " << searchDepth << "-" << maxPlyReached << " ��ֵ: " << bestMove.value << " ���: " << PosStr(bestMove.pos) << " ���ʱ: " << getTime() - lastDepthTime << " ms");
    #endif
        if (shouldBreak) break;
        lastValue = bestMove.value;
    }

    if (!board->isEmpty(bestMove.pos))
        bestMove = Move(getHighestScoreCandPos(), 0);

#ifdef VERSION_YIXIN_BOARD
    MESSAGEL("Evaluation: " << bestMove.value << " | Best Point: " << YXPos(bestMove.pos, board->size()));
#else
    MESSAGEL("��ֵ: " << bestMove.value << " | ���: " << PosStr(bestMove.pos));
#endif

    Line line;
    fetchPVLineInTT(line, bestMove.pos, searchDepth);
#ifdef VERSION_YIXIN_BOARD
    MESSAGEL("BestLine: " << line.YXPrint(board->size()));
#else
    MESSAGEL("���·��: " << line);
#endif

    bool hasLose = false;
    FOR_EVERY_POSITION_POS(p) {
        if (cell(p).isLose) {
            if (!hasLose) {
                MESSAGES_BEGIN;
            #ifdef VERSION_YIXIN_BOARD
                MESSAGES("Lose Points: ");
            #else
                MESSAGES("�ذܵ�: ");
            #endif
                hasLose = true;
            }
        #ifdef VERSION_YIXIN_BOARD
            MESSAGES(" " << YXPos(p, board->size()));
        #else
            MESSAGES(" " << PosStr(p));
        #endif

        }
    }
    if (hasLose) MESSAGES_END;

    return bestMove.pos;
}

Move AI::alphabeta_root(int depth, int alpha, int beta) {
    node++;

    Move best;
    TTEntry * tte;	// �����û���
#ifdef Hash_Probe
    if (hashTable->probe(board->getZobristKey(), tte)) {
        if (tte->isValid(depth, alpha, beta, ply))
            return tte->bestMove(ply);
        else
            best.pos = tte->bestPos();
    }
#endif

    //���ɸ��ڵ��ŷ�
    MoveList & moveList = moveLists[0];
    moveList.hashMove = best.pos;
    WinState state = genMove_Root(moveList);
    MoveList::MoveIterator move = moveList.begin();

    if (state == State_Win || state == State_Lose) {
        return *move;
    } else if (moveList.moveCount() == 1) {
        terminateAI = true;
        TTEntry * tte;
        best.value = hashTable->probe(board->getZobristKey(), tte) ? tte->value(ply) : evaluate();
        best.pos = move->pos;
        return best;
    }

    nodeExpended++;
    float newDepth = (float)depth - getDepthReduction();
    HashFlag hashFlag = Hash_Alpha;
    int value;
    int availableCount = 0;

    rawStaticEval[ply] = rawEvaluate();
    minEvalPly = depth;

    do {
        if (cell(move->pos).isLose) {
            DEBUGL("PVS����" << PosStr(move->pos) << ": Lose");
            ANALYSIS("LOST", move->pos);
            continue;
        }

        lastSelfP4 = cell(move->pos).pattern4[SELF];
        lastOppoP4 = cell(move->pos).pattern4[OPPO];

        ANALYSIS("POS", move->pos);

        makeMove(move->pos);
        if (hashFlag == Hash_PV) {
            value = -alphabeta<NonPV>(newDepth, -(alpha + 1), -alpha, true);
            if (value > alpha && value < beta)
                value = -alphabeta<PV>(newDepth, -beta, -alpha, false);
        } else {
            value = -alphabeta<PV>(newDepth, -beta, -alpha, false);
        }
        undoMove();

        ANALYSIS("DONE", move->pos);

        if (terminateAI) {
            // ��ֹʱ,��δ������ķ�֧����
            if (availableCount == 0) { // �����δ�ѵ�����ѡ��
                if (best.value >= -WIN_MAX) { // ǰ���ѡ�㶼�Ǳذ�,ѡ��ǰѡ��
                    best.value = -INF;
                    best.pos = move->pos;
                } else {  // �����һ��ѡ�㶼û������,�˲�����
                    best.pos = NullPos;
                }
            }
            break;
        }

        DEBUGL("PVS����" << PosStr(move->pos) << "  Ԥ��:" << move->value << "  ���:" << value << " Best.value = " << best.value);

        if (value <= -WIN_MIN) {
            cell(move->pos).isLose = true;
            move->value = -value;
            ANALYSIS("LOST", move->pos);
        } else {
            availableCount++;
            move->value -= 100;
        }

        if (value > best.value) {
            best.value = value;
            best.pos = move->pos;
            ANALYSIS("BEST", best.pos);
            BestMoveChangeCount++;
            move->value = value + 1000;
            // ���ڵ��value�����beta��
            if (value > alpha) {
                hashFlag = Hash_PV;
                alpha = value;
            }
        }

        if (availableCount > 0 && timeUsed() > timeForTurn() - TIME_RESERVED_PER_MOVE)
            terminateAI = true;

    } while (++move < moveList.end());

    if (availableCount <= 1) terminateAI = true;

#ifdef Hash_Record
    if (!terminateAI) {
        assert(board->isEmpty(best.pos));
        tte->save(board->getZobristKey(), best, depth, hashFlag, ply, hashTable->getGeneration());
    }
#endif
    ANALYSIS("REFRESH", NullPos);

    return best;
}

template <NodeType NT>
int AI::alphabeta(float depth, int alpha, int beta, bool cutNode) {
    const bool PvNode = NT == PV;
    assert(PvNode || (alpha == beta - 1));
    assert(!(PvNode && cutNode));

    node++;

    // Step 01. Mate Distance Purning
    alpha = MAX(-WIN_MAX + ply, alpha);
    beta = MIN(WIN_MAX - ply - 1, beta);
    if (alpha >= beta) return alpha;

    // Step 02. ��ǰʤ���ж�
    int staticEval = quickWinCheck();
    if (staticEval != 0) {
        updateMaxPlyReached(ply);
        return staticEval;
    }

    // Step 03. ƽ���ж�
    if (board->getMoveLeftCount() <= 1) return 0;

    // Step 04. ���澲̬��ֵ
    staticEval = evaluate();
    const Piece self = SELF, oppo = OPPO;
    int oppo5 = p4Count[oppo][A_FIVE];          // �Է��ĳ�����
    int oppo4 = oppo5 + p4Count[oppo][B_FLEX4]; // �Է��ĳ��ĺͻ�����

    // Step 05. Ҷ�ӽڵ��ֵ
    
    if (depth <= 0
        && ply >= minEvalPly) {   // �Ƿ�ﵽ��Ͳ���
        updateMaxPlyReached(ply);

    #ifdef VCF_Leaf
        if (staticEval >= beta) {  // Ϊ�Է���ɱ
            if (oppo5 > 0) {  // �Է�������VCF
                VCFMaxPly = ply + MAX_VCF_PLY;
                attackerPiece = oppo;
                int mateValue = quickVCFSearch();
                if (mateValue <= -WIN_MIN) return mateValue;
            }
        } else {
            if (oppo5 == 0) {  // �ҿ��Գ���VCF
                VCFMaxPly = ply + MAX_VCF_PLY;
                attackerPiece = self;
                int mateValue = quickVCFSearch();
                if (mateValue >= WIN_MIN) return mateValue;
            } else if (staticEval >= alpha) {  // �Է�������VCF
                VCFMaxPly = ply + MAX_VCF_PLY;
                attackerPiece = oppo;
                int mateValue = quickVCFSearch();
                if (mateValue <= -WIN_MIN) return mateValue;
            }
        }
    #endif
        return staticEval;
    }
    
    TTEntry * tte;
    Pos ttMove = NullPos;  // �û�����¼���ŷ�
    bool ttHit;            // �Ƿ������û���
    int ttValue;           // �û����б������һ�������Ĺ�ֵ
    bool pvExact;          // �Ƿ�Ϊȷ����PV�ڵ�
    // Step 06. �����û���(Transposition Table Probe)
#ifdef Hash_Probe
    if (ttHit = hashTable->probe(board->getZobristKey(), tte)) {
        if (ply > singularExtensionPly) {   // ��������ʱ�������û����ض�
            int ttDepth = PvNode ? (int)roundf(depth) + 1 : (int)roundf(depth);
            ttValue = tte->value(ply);

            if (tte->isValid(ttDepth, alpha, beta, ply)) {
                return ttValue;
            } else {
                ttMove = tte->bestPos();
                // �û����б����ֵ�Ⱦ�̬��ֵ����ȷ
                if (tte->flag() == (ttValue > staticEval ? Hash_Beta : Hash_Alpha))
                    staticEval = ttValue;
            }
        }
    }
#endif
    pvExact = PvNode ? isPvExact[ply - 1] && (isPvExact[ply] = ttHit && tte->flag() == Hash_PV)
        : (isPvExact[ply] = false);

    nodeExpended++;

    // Step 07. ��ʱ�ж�(Time Control)
    static int cnt = 0;
    if (--cnt < 0) {
        cnt = 3000;
        if (timeUsed() > timeForTurnMax()) 
            terminateAI = true;
    }

    rawStaticEval[ply] = rawEvaluate();    // ����ÿһ���ԭʼ��ֵ

    // Singular Extension skip all early purning (�������е����ڼ�֦)
    if (excludedMove) goto MoveLoops;

    // Step 08. Razoring (skipped when oppo4 > 0)
#ifdef Razoring
    if (!PvNode && depth < RazoringDepth) {
        if (staticEval + RazoringMargin[MAX((int)floorf(depth), 0)] < alpha) {
            return staticEval;
        }
    }
#endif

    // Step 09. Futility Purning : child node
#ifdef Futility_Pruning
    if (depth < FutilityDepth) {
        if (staticEval - FutilityMargin[MAX((int)floorf(depth), 0)] >= beta)
            return staticEval;
    }
#endif

    // Step 10. �ڲ���������(IID)
#ifdef Internal_Iterative_Deepening
    if (PvNode && !board->isEmpty(ttMove) && depth >= IIDMinDepth && oppo4 == 0) {
        TTEntry * tteTemp;
        alphabeta<NT>(depth * (2.f / 3.f), -beta, -alpha, cutNode);
        if (hashTable->probe(board->getZobristKey(), tteTemp)) {
            ttMove = tteTemp->bestPos();
        }
        assert(board->isEmpty(ttMove));
    }
#endif

MoveLoops:

    // Step 11. ѭ��ȫ���ŷ�
    assert(ply >= 0 && ply < MAX_PLY - 1);
    MoveList & moveList = moveLists[excludedMove ? ply + 1 : ply];
    moveList.init(ttMove);

    HashFlag hashFlag = Hash_Alpha;
    Move best;
    Pos move;
    int value;
    int branch = 0, maxBranch = getMaxBranch(ply);
    float newDepth = depth - (depth < 0 ? MIN(1.f, getDepthReduction())
                                        : getDepthReduction()); // ������ȵݼ�
    assert(best.value >= SHRT_MIN);

    bool singularExtensionNode = depth >= 8 && oppo5 == 0 && !excludedMove && ttHit  // �������ݹ����singular extension search
        && tte->flag() == Hash_Beta && tte->depth() >= (int)roundf(depth) - 3;

    Pos last1 = board->getMoveBackWard(1);
    Pos last2 = board->getMoveBackWard(2);

    while (moveNext(moveList, move)) {
        if (move == excludedMove && ply == singularExtensionPly) continue;
        branch++;

        lastSelfP4 = cell(move).pattern4[self];
        lastOppoP4 = cell(move).pattern4[oppo];

        // Step 12. ����ʽ��֦
        int distance1 = distance(move, last1);
        int distance2 = distance(move, last2);
        bool isNear1 = distance1 <= CONTINUES_NEIGHBOR || isInLine(move, last1) && distance1 <= CONTINUES_DISTANCE;
        bool isNear2 = distance2 <= CONTINUES_NEIGHBOR || isInLine(move, last2) && distance2 <= CONTINUES_DISTANCE;;
        bool noImportantP4 = lastSelfP4 == NONE && lastOppoP4 == NONE;
        // ��֧����֦(���ܻ������ɱ)
        if (noImportantP4) {
            if (best.value > -WIN_MIN) {
                if (branch > maxBranch) continue;
            } else {
                if (branch > MAX(maxBranch, MAX_WINNING_CHECK_BRANCH)) {
                    bool isNear3 = distance(move, board->getMoveBackWard(3)) <= CONTINUES_DISTANCE;
                    if (!(isNear1 && isNear3))
                        continue;
                }
            }

            // ���ؼ�֦
            if (!PvNode && best.value > -WIN_MIN) {
                // near-horizon
                if (ply >= minEvalPly - 2 && newDepth <= 1) {
                    const int MinPreFrontierBranch = isNear1 ? (isNear2 ? 24 : 18) : 10;
                    if (branch >= MinPreFrontierBranch) continue;
                }
            }
        }

        float moveDepth = newDepth;

        // Step 13. Singular extension search(��������)
        // ������һ���ŷ�����beta�������ŷ�����������(alpha-s, beta-s)ʱ��fail low,
        // ˵������ŷ��ǵ�һ��,��Ҫ����
    #ifdef Singular_Extension
        if (singularExtensionNode && move == ttMove) {
            int rBeta = MAX(ttValue - (int)ceilf(SEBetaMargin * depth), -WIN_MAX);
            float SEDepth = depth * 0.5f;
            int minEvalPlyTemp = minEvalPly;

            excludedMove = move;
            minEvalPly = 0;
            singularExtensionPly = ply;

            value = alphabeta<NonPV>(SEDepth, rBeta - 1, rBeta, cutNode);

            singularExtensionPly = -1;
            minEvalPly = minEvalPlyTemp;
            excludedMove = NullPos;

            if (value < rBeta)
                moveDepth += 1.0f;
        }
    #endif

        // Step 14. �³��ŷ�(Make move)
        makeMove(move);

        bool doFullDepthSearch = !PvNode || branch > 1;

        // Step 15. LMR(Late Move Reduction)
    #ifdef Late_Move_Reduction
        const int LMR_MinBranch = PvNode ? 30 : 20;
        if (depth >= 3 && oppo4 == 0 && branch >= LMR_MinBranch) {
            float reduction = 0.f;
            reduction = (branch - LMR_MinBranch) * 0.5f;
            
            if (pvExact) 
                reduction -= 1;

            if (selfP4 >= H_FLEX3)
                reduction += 1;

            if (oppoP4 >= H_FLEX3)
                reduction -= 1;
            
            if (cutNode)
                reduction += 2;

            reduction = MIN(reduction, moveDepth - newDepth * 0.4f);
            
            if (reduction > 0) {
                value = -alphabeta<false>(moveDepth - reduction, -(alpha + 1), -alpha, !cutNode);

                if (value <= alpha)
                    doFullDepthSearch = false;
            }
        }
    #endif

        // Step 16. ��ȫ���� (Full depth search when no cut exist and LMR failed)
        if (doFullDepthSearch) {
            value = -alphabeta<NonPV>(moveDepth, -(alpha + 1), -alpha, !cutNode);
        }

        // Step 17. PV node full search.
        if (PvNode && (branch <= 1 || (value > alpha && value < beta))) {
            value = -alphabeta<PV>(moveDepth, -beta, -alpha, false);
        }

        // Step 18. �����ŷ�(Undo move)
        undoMove();

        // Step 19. ��ǰ��ֹ����(��ֹʱ����δ������ķ�֧����)
        if (terminateAI) break;

        // Step 20. ��������ŷ�
        if (value > best.value) {
            best.value = value;
            best.pos = move;
            if (value >= beta) {
                hashFlag = Hash_Beta;
                break;
            } else if (value > alpha) {
                hashFlag = Hash_PV;
                alpha = value;
            }
        }
    }

    assert(terminateAI || best.value >= -WIN_MAX && best.value <= WIN_MAX);

    // Step 21. �û�������(Transposition Table Record)
    // ��ǰ��ֹ��Singular Extensionʱ����
#ifdef Hash_Record
    if (!terminateAI && !excludedMove)
        tte->save(board->getZobristKey(), best, (int)roundf(depth), hashFlag, ply, hashTable->getGeneration());
#endif
    return best.value;
}

template int AI::alphabeta<PV>(float depth, int alpha, int beta, bool cutNode);
template int AI::alphabeta<NonPV>(float depth, int alpha, int beta, bool cutNode);

// ���� 0 ���û���ҵ� VCF
template <bool Root>
int AI::quickVCFSearch() {
    assert(attackerPiece == Black || attackerPiece == White);
    node++;

    const Piece self = SELF, oppo = OPPO;
    int value;
    TTEntry * tte;

    if (Root) {
    #ifdef Hash_Probe
        if (hashTable->probe(board->getZobristKey(), tte)) {
            if (tte->isMate())
                return tte->value(ply);
        }
    #endif
    }

    // ��ǰʤ���ж�
    value = quickWinCheck();
    if (value != 0) {
        updateMaxPlyReached(ply);
        return value;
    }

    // VCF����������
    if (ply > VCFMaxPly) {
        updateMaxPlyReached(ply);
        return 0;
    }

    if (p4Count[oppo][A_FIVE] > 0) {  // VCF�ڵ��ж���һ���Ƿ��ǳ���
        Pos move = getCostPosAgainstB4(board->getLastMove(), oppo);
        if (self == attackerPiece) {  // ������ǹ�����,��������
            if (cell(move).pattern4[self] < E_BLOCK4) {  // ����¶Է��ĳ��ĵ��岻���ҵĳ���
                updateMaxPlyReached(ply);
                return 0;
            }
        }

        makeMove<VC>(move);
        value = -quickVCFSearch<Root>();
        undoMove<VC>();

        return value;
    }

    assert(p4Count[oppo][A_FIVE] == 0);
    assert(self == attackerPiece);

    // ����������Ƿ���VCF�ŷ�
    if (p4Count[self][C_BLOCK4_FLEX3] == 0 && p4Count[self][D_BLOCK4_PLUS] == 0 && p4Count[self][E_BLOCK4] == 0) {
        updateMaxPlyReached(ply);
        return 0;
    }

    // ��ʱ�ж�
    static int cnt = 0;
    if (--cnt < 0) {
        cnt = 7000;
        if (timeUsed() > timeForTurnMax()) 
            return terminateAI = true, 0;
    }

    nodeExpended++;

    const GenLevel Level = Root ? InFullBoard : InLine;
    assert(ply >= 0 && ply < MAX_PLY);
    MoveList & moveList = moveLists[ply];
    moveList.init_GenAllMoves();
    if (Root)        // �������ɲ㼶����VCF�ŷ�
        genMoves_VCF(moveList);
    else
        genContinueMoves_VCF(moveList, RANGE_LINE_4, 32);

    if (moveList.moveCount() == 0)  // �ж��Ƿ�������Ľ����ŷ�
        return 0;

    sort(moveList.begin(), moveList.end(), std::greater<Move>());  // VCF�ŷ�����

    Move best;
    Pos attMove, defMove;
    assert(moveList.n == 0);

    do {
        attMove = moveList.moves[moveList.n].pos;
        
        makeMove<VC>(attMove);

        defMove = getCostPosAgainstB4(attMove, self);
        assert(cell(defMove).pattern4[self] == A_FIVE);
        assert(p4Count[self][A_FIVE] > 1 || defMove == findPosByPattern4(self, A_FIVE));

        makeMove<VC>(defMove);
        value = quickVCFSearch<false>();  // ����move�󲻷���
        undoMove<VC>();

        undoMove<VC>();

        if (value > best.value) {
            best.value = value;
            best.pos = attMove;
            if (value >= WIN_MIN) break;
        }
        if (terminateAI) break;

    #ifdef VCF_Branch_Limit
        if (moveList.n >= MAX_VCF_BRANCH - 1) break;
    #endif
    } while (++moveList.n < moveList.moveCount());

    if (best.value <= -WIN_MIN)  // ��Ϊ������������ֻ��VCF�ŷ�����ʹ����Ҳ������������
        best.value = 0;

#ifdef Hash_Record
    if (Root) {
        if (!terminateAI && best.value >= WIN_MIN) {
            tte->save(board->getZobristKey(), best, 0, HashFlag::Hash_PV, ply, hashTable->getGeneration());
        }
    }
#endif

    return best.value;
}

template int AI::quickVCFSearch<true>();
template int AI::quickVCFSearch<false>();

WinState AI::genMove_Root(MoveList & moveList) {
    switch (moveList.phase) {
    case MoveList::AllMoves:
        if (moveList.moveCount() > 0) {
            // ���ڵ������Ҫ����ԭʼ˳��
            stable_sort(moveList.begin(), moveList.end(), std::greater<Move>());
            return State_Unknown;
        } else
            moveList.phase = MoveList::GenAllMoves;
    case MoveList::GenAllMoves:
    {
        Piece self = SELF, oppo = OPPO;
        if (p4Count[self][A_FIVE] > 0) {
            moveList.addMove(findPosByPattern4(self, A_FIVE), WIN_MAX);
            return State_Win;
        } else if (p4Count[oppo][A_FIVE] >= 2) {
            moveList.addMove(findPosByPattern4(oppo, A_FIVE), -WIN_MAX + 1);
            return State_Lose;
        } else if (p4Count[oppo][A_FIVE] == 1) {
            moveList.addMove(findPosByPattern4(oppo, A_FIVE), 0);
            return State_Unknown;
        } else {
            if (p4Count[self][B_FLEX4] > 0) {
                moveList.addMove(findPosByPattern4(self, B_FLEX4), WIN_MAX - 2);
                return State_Win;
            } else if (p4Count[oppo][B_FLEX4] > 0) {
                genMoves_defence(moveList);
            } else {
                genMoves(moveList);
            }
        }
        // ��HashMove�ᵽ��ǰ
        if (board->isEmpty(moveList.hashMove)) {
            for (MoveList::MoveIterator move = moveList.begin(); move != moveList.end(); move++)
                if (move->pos == moveList.hashMove) {
                    move->value += 10000;
                    break;
                }
        }
        assert(moveList.moveCount() > 0);
        sort(moveList.begin(), moveList.end(), std::greater<Move>());
        moveList.phase = MoveList::AllMoves;
        return State_Unknown;
    }
    default: 
        assert(false);
        return State_Unknown;
    }
}

bool AI::moveNext(MoveList & moveList, Pos & pos) {
    switch (moveList.phase) {
    case MoveList::HashMove:
        moveList.phase = MoveList::GenAllMoves;
        if (board->isEmpty(moveList.hashMove)) {
            pos = moveList.hashMove;
            return true;
        }
    case MoveList::GenAllMoves:
    {
        moveList.phase = MoveList::AllMoves;
        if (p4Count[OPPO][A_FIVE] > 0) {
            pos = findPosByPattern4(OPPO, A_FIVE);
        } else {
            if (p4Count[OPPO][B_FLEX4] > 0) {
                genMoves_defence(moveList);
            } else {
                genMoves(moveList);
            }
            assert(moveList.moveCount() > 0);
            sort(moveList.begin(), moveList.end(), std::greater<Move>());
            pos = moveList.moves.front().pos;
        }
        return true;
    }
    case MoveList::AllMoves:
        if (++moveList.n >= moveList.moveCount())
            return false;
        pos = moveList.moves[moveList.n].pos;
        return true;
    default: assert(false); return false;
    }
}

// ����ȫ���ŷ�
void AI::genMoves(MoveList & moveList) {
    Piece self = SELF;
    int score;

    FOR_EVERY_CAND_POS(p) {
        score = cell(p).getScore(self);
        moveList.addMove(p, score);
    }
}
// ����ȫ�������������ŷ����������ĳ��ģ��Է����ĺͳ��ģ�
void AI::genMoves_defence(MoveList & moveList) {
    Piece self = SELF, oppo = OPPO;
    static set<Pos> defence;
    defence.clear();
    assert(p4Count[OPPO][B_FLEX4] > 0);

    FOR_EVERY_CAND_POS(p) {
        if (cell(p).pattern4[oppo] == B_FLEX4) {
            getAllCostPosAgainstF3(p, oppo, defence);
        } else if (cell(p).pattern4[self] >= E_BLOCK4) {
            moveList.addMove(p, cell(p).getScore(self));
        }
    }

    set<Pos>::iterator it2 = defence.end();
    for (set<Pos>::iterator it1 = defence.begin(); it1 != it2; it1++) {
        moveList.addMove(*it1, cell(*it1).getScore(oppo) + 10000);
    }
    assert(moveList.moveCount() > 0);
}
// ֻ��������/�����ڵĳ����ŷ�
void AI::genMoves_VCF(MoveList & moveList) {
    Piece self = SELF;

    FOR_EVERY_CAND_POS(p) {
        if (cell(p).pattern4[self] >= E_BLOCK4) {
            moveList.addMove(p, cell(p).getScore_VC(self));
        }
    }
}
// �����������Ľ����ŷ�
void AI::genContinueMoves_VCF(MoveList & moveList, const short * range, int n) {
    Piece self = SELF;
    Pos last = board->getMoveBackWard(2), p;

    if (last == NullPos) return;

    for (int i = 0; i < n; i++) {
        p = last + range[i];

        if (board->isEmpty(p)) {
            if (cell(p).pattern4[self] >= E_BLOCK4) {
                moveList.addMove(p, cell(p).getScore_VC(self));
            }
        }
    }
}

inline int AI::evaluate() {
    assert(p4Count[SELF][A_FIVE] == 0);

    // ȫ������
    int value = eval[SELF] - eval[OPPO];

    value = (value - rawStaticEval[ply - 1]) / 2;   // ��һ������ķ��������һ���Ǹ���

    return value;
}

inline int AI::rawEvaluate() {
    return eval[SELF] - eval[OPPO];
}

int AI::quickWinCheck() {
    Piece self = SELF, oppo = OPPO;
#ifdef Win_Check_FLEX3_2X
    bool has_FLEX3_2X = lastSelfP4 == F_FLEX3_2X;
#else
    bool has_FLEX3_2X = false;
#endif
    has_FLEX3_2X = false;

    if (p4Count[self][A_FIVE] >= 1) return WIN_MAX - ply;
    if (p4Count[oppo][A_FIVE] >= 2) return -WIN_MAX + ply + 1;
    if (p4Count[oppo][A_FIVE] == 1) return 0;
    if (p4Count[self][B_FLEX4] >= 1) return WIN_MAX - ply - 2;

    int self_C_count = p4Count[self][C_BLOCK4_FLEX3];
    if (self_C_count >= 1) {
        if (p4Count[oppo][B_FLEX4] == 0 && p4Count[oppo][C_BLOCK4_FLEX3] == 0 && p4Count[oppo][D_BLOCK4_PLUS] == 0 && p4Count[oppo][E_BLOCK4] == 0)
            return WIN_MAX - ply - 4;
        FOR_EVERY_CAND_POS(p) {    // ��43���ͷ����Ŀ�����֤(��̬�ж�)
            if (cell(p).pattern4[self] == C_BLOCK4_FLEX3) {
                makeMove<VC>(p);
                Pos defMove = getCostPosAgainstB4(p, self);
                if (cell(defMove).pattern4[oppo] < E_BLOCK4) {
                    undoMove<VC>();
                    return WIN_MAX - ply - 4;
                }
                undoMove<VC>();
                if (--self_C_count <= 0) goto Check_Flex3;
            }
        }
    }
Check_Flex3:
    if (p4Count[self][F_FLEX3_2X] >= 1) {
        if (p4Count[oppo][B_FLEX4] == 0 && p4Count[oppo][C_BLOCK4_FLEX3] == 0 && p4Count[oppo][D_BLOCK4_PLUS] == 0 && p4Count[oppo][E_BLOCK4] == 0)
            return WIN_MAX - ply - 4;
    }

#ifdef Win_Check_FLEX3_2X
    if (has_FLEX3_2X) { // �Է���������������
        assert(p4Count[oppo][B_FLEX4] > 0);

        // �ȼ������������ǲ����Ѿ��жϹ���
        TTEntry * tte;
        if (!hashTable->probe(board->getZobristKey(), tte)) { // �����û�жϹ�
            int value = quickDefenceCheck();
            if (value <= -WIN_MIN) return value;
        }
    }
#endif
    return 0;
}

int AI::quickDefenceCheck() {
    Piece self = SELF, oppo = OPPO;
    int oppoB_count = p4Count[oppo][B_FLEX4];
    assert(oppoB_count > 0);
    size_t selfB4_count = 0;
    static vector<Pos> self_BLOCK4;

    while (p4Count[self][D_BLOCK4_PLUS] + p4Count[self][E_BLOCK4] > 0) { // һֱ����ֱ��û�п��Գ��ĵ�ѡ��
        self_BLOCK4.clear();
        FOR_EVERY_CAND_POS(p) {
            Pattern4 p4 = cell(p).pattern4[self];
            if (p4 >= E_BLOCK4 && p4 != B_FLEX4) {
                Cell & c = cell(p);
                int dir;
                for (dir = 0; dir < 4; dir++) {
                    Pattern pattern = c.pattern[self][dir];
                    if (pattern >= B4) {
                        Pos pos;
                        int i;
                        if (pattern == F5) pos = p;
                        else {
                            if (cell(pos = p - D[dir]).pattern4[self] == A_FIVE);
                            else if (cell(pos = p + D[dir]).pattern4[self] == A_FIVE);
                            else break;
                        }
                        for (i = 1; i <= 7; i++) {
                            pos -= D[dir];
                            Piece piece = board->get(pos);
                            if (piece != self) break;
                        }
                        if (i > 7) continue;
                        for (i = 1; i <= 7; i++) {
                            pos += D[dir];
                            Piece piece = board->get(pos);
                            if (piece != self) break;
                        }
                        if (i > 7) continue;
                        break;
                    }
                }
                if (dir < 4) self_BLOCK4.push_back(p);
            }
        }
        if (self_BLOCK4.size() == 0) break;
        for (size_t i = 0; i < self_BLOCK4.size(); i++)
            makeMove<MuiltVC>(self_BLOCK4[i]);
        selfB4_count += self_BLOCK4.size();
        if (p4Count[self][B_FLEX4] > 0 || p4Count[self][C_BLOCK4_FLEX3] > 0) {
            oppoB_count = 0;
            break;  // ����VCF(α)ʱ�Լ����˻���
        }
    }
    if (oppoB_count > 0 && p4Count[oppo][B_FLEX4] == oppoB_count)
        oppoB_count = INF;

    for (size_t i = 0; i < selfB4_count; i++)
        undoMove<MuiltVC>();

    // ��Ϊÿ�����Ļ����2����,���ع��Ʋ�������2
    if (oppoB_count == INF)
        return -WIN_MAX + ply + 3 + (int)selfB4_count / 2;  
    return 0;
}

float AI::getDepthReduction() {
    Piece self = SELF, oppo = OPPO;

    int branchCount;

    if (p4Count[oppo][A_FIVE] == 1) {
        branchCount = 1;
    } else {
        int oppo_B_Count = p4Count[oppo][B_FLEX4];
        if (oppo_B_Count > 0) {
            branchCount = oppo_B_Count == 1 ? 3 : oppo_B_Count * 2;
        } else {
            branchCount = 0;
            FOR_EVERY_CAND_POS(p) {
                branchCount++;
            }
        }
    }
    assert(branchCount > 0);

    return logf((float)branchCount /*+ 1e-3f*/) * depthReductionBase;
}

void AI::fetchPVLineInTT(Line & line, Pos firstMove, int maxDepth) {
    if (maxDepth <= 0) return;
    line.pushMove(firstMove);
    makeMove(firstMove);
    TTEntry * tte;
    if (hashTable->probe(board->getZobristKey(), tte)) {
        Pos next = tte->bestPos();
        if (board->isEmpty(next))
            fetchPVLineInTT(line, next, maxDepth - 1);
    }
    undoMove();
}

void AI::tryReadConfig(string path) {
    std::ifstream file(path, std::ifstream::in);
    if (!file) return;
    const int LineSize = 1000;
    char line[LineSize];

    int override;
    file.getline(line, LineSize);
    sscanf_s(line, "Override:%d", &override);
    if (override != 1) {
        file.close();
        return;
    }
    override = 0;

    while (!file.eof()) {
        file.getline(line, LineSize);

        if (strncmp(line, "Eval:", 10) == 0) {
            for (int i = 0; i < 3876; i++)
                file >> Value[i];
            override++;
        } else if (strncmp(line, "Score:", 6) == 0) {
            for (int i = 0; i < 3876; i++)
                file >> Score[i];
            override++;
        } else if (strncmp(line, "ExtensionCoefficient:", 21) == 0) {
            int ExtensionNumBase;
            sscanf_s(line, "ExtensionCoefficient:%d", &ExtensionNumBase);
            depthReductionBase = ExtensionNumBase <= 1 ? 1e6f : 1.f / logf((float)ExtensionNumBase);
            override++;
        } else if (strncmp(line, "UseOpeningBook:", 15) == 0) {
            int opening;
            sscanf_s(line, "UseOpeningBook:%d", &opening);
            useOpeningBook = opening == 1;
            override++;
        } else if (strncmp(line, "FutilityPurningMargin:", 22) == 0) {
            FutilityDepth = sscanf_s(line, "FutilityPurningMargin:%d%d%d%d", FutilityMargin, FutilityMargin + 1, FutilityMargin + 2, FutilityMargin + 3);
            if (FutilityDepth > FUTILITY_MAX_DEPTH) FutilityDepth = FUTILITY_MAX_DEPTH;
            override++;
        } else if (strncmp(line, "RazoringMargin:", 15) == 0) {
            sscanf_s(line, "RazoringMargin:%d%d%d%d", RazoringMargin, RazoringMargin + 1, RazoringMargin + 2, RazoringMargin + 3);
            if (RazoringDepth > RAZORING_MAX_DEPTH) RazoringDepth = RAZORING_MAX_DEPTH;
            override++;
        } else if (strncmp(line, "IIDMinDepth:", 12) == 0) {
            sscanf_s(line, "IIDMinDepth:%d", &IIDMinDepth);
            override++;
        } else if (strncmp(line, "SEBetaMargin:", 13) == 0) {
            sscanf_s(line, "SEBetaMargin:%f", &SEBetaMargin);
            override++;
        } else if (strncmp(line, "ReloadConfigOnEachMove:", 23) == 0) {
            int reload;
            sscanf_s(line, "ReloadConfigOnEachMove:%d", &reload);
            reloadConfig = reload == 1;
            override++;
        }
    }
    file.close();

#ifdef VERSION_YIXIN_BOARD
    MESSAGEL("Custom config has been read, " << override << " properties changed.");
#else
    MESSAGEL("Custom config has been read, " << override << " properties changed.");
#endif
}
