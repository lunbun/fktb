#include "movegen.h"

#include <cassert>

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
    // Serializes all moves in a bitboard, both quiet and captures (will not serialize quiet if in ExcludeQuiet mode)
    void serializeBitboard(Square from, Bitboard bitboard);

    void generatePawnMoves(Square square);
    void generateAllPawnMoves();

    void generateCastlingMoves();

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
    assert(!ExcludeQuiet);
    while (quiet) {
        Square to = quiet.bsfReset();
        this->list_.push({ from, to, MoveFlag::Quiet });
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializeCaptures(Square from, Bitboard captures) {
    while (captures) {
        Square to = captures.bsfReset();
        this->list_.push({ from, to, MoveFlag::Capture });
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializePromotions(Square from, Bitboard promotions) {
    while (promotions) {
        Square to = promotions.bsfReset();
        this->list_.push({ from, to, MoveFlag::KnightPromotion });
        this->list_.push({ from, to, MoveFlag::BishopPromotion });
        this->list_.push({ from, to, MoveFlag::RookPromotion });
        this->list_.push({ from, to, MoveFlag::QueenPromotion });
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializeBitboard(Square from, Bitboard bitboard) {
    if constexpr (!ExcludeQuiet) {
        this->serializeQuiet(from, bitboard & this->empty_);
    }
    this->serializeCaptures(from, bitboard & this->enemy_);
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::generatePawnMoves(Square square) {
    Bitboard empty = this->empty_;

    if constexpr (Side == Color::White) {
        // Single push (quiet, but needed to check for promotions, which are not quiet)
        Bitboard singlePush = ((1ULL << square) << 8) & empty;
        Bitboard promotions = singlePush & Bitboards::Rank8;

        this->serializePromotions(square, promotions);

        if constexpr (!ExcludeQuiet) {
            this->serializeQuiet(square, singlePush ^ promotions);

            // Double push (always quiet)
            Bitboard doublePush = (singlePush << 8) & empty & Bitboards::Rank4;
            this->serializeQuiet(square, doublePush);
        }
    } else {
        // Single push (quiet, but needed to check for promotions, which are not quiet)
        Bitboard singlePush = ((1ULL << square) >> 8) & empty;
        Bitboard promotions = singlePush & Bitboards::Rank1;

        this->serializePromotions(square, promotions);

        if constexpr (!ExcludeQuiet) {
            this->serializeQuiet(square, singlePush ^ promotions);

            // Double push (always quiet)
            Bitboard doublePush = (singlePush >> 8) & empty & Bitboards::Rank5;
            this->serializeQuiet(square, doublePush);
        }
    }

    Bitboard attacks = Bitboards::pawn<Side>(square) & this->enemy_;
    this->serializeCaptures(square, attacks);
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::generateAllPawnMoves() {
    Bitboard pawns = this->board_.template bitboard<Side>(PieceType::Pawn);
    while (pawns) {
        Square square = pawns.bsfReset();
        this->generatePawnMoves(square);
    }
}

template<Color Side, bool ExcludeQuiet>
void MoveGenerator<Side, ExcludeQuiet>::generateCastlingMoves() {
    assert(!ExcludeQuiet);
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

template<Color Side, bool ExcludeQuiet>
template<Bitboard (*GenerateAttacks)(Square)>
INLINE void MoveGenerator<Side, ExcludeQuiet>::generateOffsetMoves(Piece piece) {
    Bitboard pieces = this->board_.template bitboard<Side>(piece.type());
    while (pieces) {
        Square square = pieces.bsfReset();

        Bitboard attacks = GenerateAttacks(square);
        this->serializeBitboard(square, attacks);
    }
}

template<Color Side, bool ExcludeQuiet>
template<Bitboard (*GenerateAttacks)(Square, Bitboard)>
INLINE void MoveGenerator<Side, ExcludeQuiet>::generateSlidingMoves(Piece piece) {
    Bitboard pieces = this->board_.template bitboard<Side>(piece.type());
    while (pieces) {
        Square square = pieces.bsfReset();

        Bitboard attacks = GenerateAttacks(square, this->occupied_);
        this->serializeBitboard(square, attacks);
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE MoveEntry *MoveGenerator<Side, ExcludeQuiet>::generate() {
    this->generateAllPawnMoves();
    this->generateOffsetMoves<Bitboards::knight>(Piece::knight(Side));
    this->generateSlidingMoves<Bitboards::bishop>(Piece::bishop(Side));
    this->generateSlidingMoves<Bitboards::rook>(Piece::rook(Side));
    this->generateSlidingMoves<Bitboards::queen>(Piece::queen(Side));
    this->generateOffsetMoves<Bitboards::king>(Piece::king(Side));

    if constexpr (!ExcludeQuiet) {
        this->generateCastlingMoves();
    }

    return this->list_.end();
}



template<Color Side, bool ExcludeQuiet>
MoveEntry *MoveGeneration::generate(const Board &board, MoveEntry *moves) {
    MoveGenerator<Side, ExcludeQuiet> generator(board, moves);
    return generator.generate();
}

RootMoveList MoveGeneration::generateRoot(const Board &board) {
    AlignedMoveEntry movesBuffer[MaxMoveCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(movesBuffer);

    MoveEntry *movesEnd;
    if (board.turn() == Color::White) {
        movesEnd = MoveGeneration::generate<Color::White, false>(board, movesStart);
    } else {
        movesEnd = MoveGeneration::generate<Color::Black, false>(board, movesStart);
    }

    // RootMoveList will copy the moves into its own buffer
    return { movesStart, movesEnd };
}

template MoveEntry *MoveGeneration::generate<Color::White, false>(const Board &board, MoveEntry *moves);
template MoveEntry *MoveGeneration::generate<Color::Black, false>(const Board &board, MoveEntry *moves);
template MoveEntry *MoveGeneration::generate<Color::White, true>(const Board &board, MoveEntry *moves);
template MoveEntry *MoveGeneration::generate<Color::Black, true>(const Board &board, MoveEntry *moves);
