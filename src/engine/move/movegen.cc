#include "movegen.h"

#include "move_list.h"
#include "engine/inline.h"
#include "engine/board/castling.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"
#include "engine/board/color.h"
#include "engine/board/board.h"
#include "engine/board/bitboard.h"

// Class is used for convenience so that we don't have to pass around the board, move list, and bitboards separately as
// parameters.
template<Color Side, bool ExcludeQuiet>
class MoveGenerator {
public:
    MoveGenerator() = delete;
    MoveGenerator(const Board &board, MoveEntry *moves);

    // Returns the pointer to the end of the move list.
    [[nodiscard]] MoveEntry *generate();

private:
    const Board &board_;
    MoveList list_;

    Bitboard friendly_;
    Bitboard enemy_;
    Bitboard occupied_;
    Bitboard empty_;

    void serializeQuiet(Square from, Bitboard quiet);
    void serializeCaptures(Square from, Bitboard captures);
    void serializePromotions(Square from, Bitboard promotions);
    void serializePromotionCaptures(Square from, Bitboard promoCaptures);
    // Serializes all moves in a bitboard, both quiet and captures (will not serialize quiet if in ExcludeQuiet mode)
    void serializeBitboard(Square from, Bitboard bitboard);

    void generatePawnMoves(Square square);
    void generateAllPawnMoves();

    void generateKingMoves();

    template<Bitboard (*GenerateAttacks)(Square)>
    void generateOffsetMoves(Piece piece);

    template<Bitboard (*GenerateAttacks)(Square, Bitboard)>
    void generateSlidingMoves(Piece piece);
};

template<Color Side, bool ExcludeQuiet>
MoveGenerator<Side, ExcludeQuiet>::MoveGenerator(const Board &board, MoveEntry *moves) : board_(board), list_(moves) {
    this->friendly_ = board.composite<Side>();
    this->enemy_ = board.composite<~Side>();
    this->occupied_ = this->friendly_ | this->enemy_;
    this->empty_ = ~this->occupied_;
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializeQuiet(Square from, Bitboard quiet) {
    static_assert(!ExcludeQuiet, "Cannot serialize quiet moves in ExcludeQuiet mode");
    for (Square to : quiet) {
        this->list_.push({ from, to, MoveFlag::Quiet });
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializeCaptures(Square from, Bitboard captures) {
    for (Square to : captures) {
        this->list_.push({ from, to, MoveFlag::Capture });
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializePromotions(Square from, Bitboard promotions) {
    for (Square to : promotions) {
        this->list_.push({ from, to, MoveFlag::KnightPromotion });
        this->list_.push({ from, to, MoveFlag::BishopPromotion });
        this->list_.push({ from, to, MoveFlag::RookPromotion });
        this->list_.push({ from, to, MoveFlag::QueenPromotion });
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializePromotionCaptures(Square from, Bitboard promoCaptures) {
    for (Square to : promoCaptures) {
        this->list_.push({ from, to, MoveFlag::KnightPromoCapture });
        this->list_.push({ from, to, MoveFlag::BishopPromoCapture });
        this->list_.push({ from, to, MoveFlag::RookPromoCapture });
        this->list_.push({ from, to, MoveFlag::QueenPromoCapture });
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializeBitboard(Square from, Bitboard bitboard) {
    if constexpr (!ExcludeQuiet) {
        this->serializeQuiet(from, bitboard & this->empty_);
    }
    this->serializeCaptures(from, bitboard & this->enemy_);
}

// Returns the bitboard shifted one rank forward for the given side.
template<Color Side>
INLINE Bitboard forwardOneRank(Bitboard pawn) {
    if constexpr (Side == Color::White) {
        return pawn << 8;
    } else {
        return pawn >> 8;
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::generatePawnMoves(Square square) {
    constexpr Bitboard PromotionRank = (Side == Color::White) ? Bitboards::Rank8 : Bitboards::Rank1;
    constexpr Bitboard DoublePushToRank = (Side == Color::White) ? Bitboards::Rank4 : Bitboards::Rank5;

    Bitboard empty = this->empty_;

    // Pawn pushes
    // Single push (quiet, but needed to check for promotions, which are not quiet)
    Bitboard singlePush = forwardOneRank<Side>(1ULL << square) & empty;
    Bitboard promotions = singlePush & PromotionRank;

    this->serializePromotions(square, promotions);

    if constexpr (!ExcludeQuiet) {
        this->serializeQuiet(square, singlePush ^ promotions);

        // Double push (always quiet)
        Bitboard doublePush = forwardOneRank<Side>(singlePush) & empty & DoublePushToRank;
        this->serializeQuiet(square, doublePush);
    }

    // Pawn captures
    Bitboard attacks = Bitboards::pawnAttacks<Side>(square) & this->enemy_;
    Bitboard promotionsCaptures = attacks & PromotionRank;
    Bitboard captures = attacks ^ promotionsCaptures;
    this->serializePromotionCaptures(square, promotionsCaptures);
    this->serializeCaptures(square, captures);
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::generateAllPawnMoves() {
    Bitboard pawns = this->board_.template bitboard<Side>(PieceType::Pawn);
    for (Square square : pawns) {
        this->generatePawnMoves(square);
    }
}

template<Color Side, bool ExcludeQuiet>
void MoveGenerator<Side, ExcludeQuiet>::generateKingMoves() {
    // Normal king moves
    Square king = this->board_.template king<Side>();
    Bitboard attacks = Bitboards::kingAttacks(king);
    this->serializeBitboard(king, attacks);

    // Castling moves
    if constexpr (!ExcludeQuiet) {
        if constexpr (Side == Color::White) {
            constexpr Bitboard F1G1 = Bitboards::F1 | Bitboards::G1;
            constexpr Bitboard B1C1D1 = Bitboards::B1 | Bitboards::C1 | Bitboards::D1;

            if (this->board_.castlingRights() & CastlingRights::WhiteKingSide) {
                if (!(this->occupied_ & F1G1)) {
                    this->list_.push({ Square::E1, Square::G1, MoveFlag::KingCastle });
                }
            }

            if (this->board_.castlingRights() & CastlingRights::WhiteQueenSide) {
                if (!(this->occupied_ & B1C1D1)) {
                    this->list_.push({ Square::E1, Square::C1, MoveFlag::QueenCastle });
                }
            }
        } else {
            constexpr Bitboard F8G8 = Bitboards::F8 | Bitboards::G8;
            constexpr Bitboard B8C8D8 = Bitboards::B8 | Bitboards::C8 | Bitboards::D8;

            if (this->board_.castlingRights() & CastlingRights::BlackKingSide) {
                if (!(this->occupied_ & F8G8)) {
                    this->list_.push({ Square::E8, Square::G8, MoveFlag::KingCastle });
                }
            }

            if (this->board_.castlingRights() & CastlingRights::BlackQueenSide) {
                if (!(this->occupied_ & B8C8D8)) {
                    this->list_.push({ Square::E8, Square::C8, MoveFlag::QueenCastle });
                }
            }
        }
    }
}

template<Color Side, bool ExcludeQuiet>
template<Bitboard (*GenerateAttacks)(Square)>
INLINE void MoveGenerator<Side, ExcludeQuiet>::generateOffsetMoves(Piece piece) {
    Bitboard pieces = this->board_.template bitboard<Side>(piece.type());
    for (Square square : pieces) {
        Bitboard attacks = GenerateAttacks(square);
        this->serializeBitboard(square, attacks);
    }
}

template<Color Side, bool ExcludeQuiet>
template<Bitboard (*GenerateAttacks)(Square, Bitboard)>
INLINE void MoveGenerator<Side, ExcludeQuiet>::generateSlidingMoves(Piece piece) {
    Bitboard pieces = this->board_.template bitboard<Side>(piece.type());
    for (Square square : pieces) {
        Bitboard attacks = GenerateAttacks(square, this->occupied_);
        this->serializeBitboard(square, attacks);
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE MoveEntry *MoveGenerator<Side, ExcludeQuiet>::generate() {
    this->generateAllPawnMoves();
    this->generateOffsetMoves<Bitboards::knightAttacks>(Piece::knight(Side));
    this->generateSlidingMoves<Bitboards::bishopAttacks>(Piece::bishop(Side));
    this->generateSlidingMoves<Bitboards::rookAttacks>(Piece::rook(Side));
    this->generateSlidingMoves<Bitboards::queenAttacks>(Piece::queen(Side));
    this->generateKingMoves();

    return this->list_.end();
}

template<Color Side>
INLINE MoveEntry *filterLegal(Board &board, MoveEntry *start, MoveEntry *end) {
    MoveEntry *result = start;
    // TODO: This is a very inefficient method. Implement a more efficient method.
    for (MoveEntry *entry = start; entry != end; entry++) {
        Move move = entry->move;
        MakeMoveInfo info = board.makeMove<false>(move);

        // TODO: Do not allow castling through check (or in check)
        if (!board.isInCheck<Side>()) {
            *(result++) = *entry;
        }

        board.unmakeMove<false>(move, info);
    }

    return result;
}



template<Color Side, bool ExcludeQuiet>
MoveEntry *MoveGeneration::generatePseudoLegal(const Board &board, MoveEntry *moves) {
    MoveGenerator<Side, ExcludeQuiet> generator(board, moves);
    return generator.generate();
}

template<Color Side, bool ExcludeQuiet>
MoveEntry *MoveGeneration::generateLegal(Board &board, MoveEntry *moves) {
    MoveEntry *end = MoveGeneration::generatePseudoLegal<Side, ExcludeQuiet>(board, moves);
    return filterLegal<Side>(board, moves, end);
}

RootMoveList MoveGeneration::generateLegalRoot(Board &board) {
    AlignedMoveEntry movesBuffer[MaxMoveCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(movesBuffer);

    MoveEntry *movesEnd;
    if (board.turn() == Color::White) {
        movesEnd = MoveGeneration::generateLegal<Color::White, false>(board, movesStart);
    } else {
        movesEnd = MoveGeneration::generateLegal<Color::Black, false>(board, movesStart);
    }

    // RootMoveList will copy the moves into its own buffer
    return { movesStart, movesEnd };
}

template MoveEntry *MoveGeneration::generateLegal<Color::White, false>(Board &board, MoveEntry *moves);
template MoveEntry *MoveGeneration::generateLegal<Color::Black, false>(Board &board, MoveEntry *moves);
template MoveEntry *MoveGeneration::generateLegal<Color::White, true>(Board &board, MoveEntry *moves);
template MoveEntry *MoveGeneration::generateLegal<Color::Black, true>(Board &board, MoveEntry *moves);
