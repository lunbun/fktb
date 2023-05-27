#include "movegen.h"

#include <stdexcept>

#include "bitboard.h"
#include "move_queue.h"
#include "inline.h"

template<Color Side, bool ExcludeQuiet>
MoveGenerator<Side, ExcludeQuiet>::MoveGenerator(const Board &board, MovePriorityQueueStack &moves) : board_(board),
                                                                                                      moves_(moves) {
    this->friendly_ = board.composite<Side>();
    this->enemy_ = board.composite<~Side>();
    this->occupied_ = this->friendly_ | this->enemy_;
    this->empty_ = ~this->occupied_;
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializeQuiet(Square from, Bitboard quiet) {
    if constexpr (ExcludeQuiet) {
        throw std::runtime_error("Cannot serialize quiet moves in ExcludeQuiet mode");
    }

    while (quiet) {
        Square to = quiet.bsfReset();
        this->moves_.enqueue({ from, to, MoveFlag::Quiet });
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializeCaptures(Square from, Bitboard captures) {
    while (captures) {
        Square to = captures.bsfReset();
        this->moves_.enqueue({ from, to, MoveFlag::Capture });
    }
}

template<Color Side, bool ExcludeQuiet>
INLINE void MoveGenerator<Side, ExcludeQuiet>::serializePromotions(Square from, Bitboard promotions) {
    while (promotions) {
        Square to = promotions.bsfReset();
        this->moves_.enqueue({ from, to, MoveFlag::KnightPromotion });
        this->moves_.enqueue({ from, to, MoveFlag::BishopPromotion });
        this->moves_.enqueue({ from, to, MoveFlag::RookPromotion });
        this->moves_.enqueue({ from, to, MoveFlag::QueenPromotion });
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

        this->serializeQuiet(square, singlePush ^ promotions);
        this->serializePromotions(square, promotions);

        // Double push (always quiet)
        if constexpr (!ExcludeQuiet) {
            Bitboard doublePush = (singlePush << 8) & empty & Bitboards::Rank4;
            this->serializeQuiet(square, doublePush);
        }
    } else {
        // Single push (quiet, but needed to check for promotions, which are not quiet)
        Bitboard singlePush = ((1ULL << square) >> 8) & empty;
        Bitboard promotions = singlePush & Bitboards::Rank1;

        this->serializeQuiet(square, singlePush ^ promotions);
        this->serializePromotions(square, promotions);

        // Double push (always quiet)
        if constexpr (!ExcludeQuiet) {
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
void MoveGenerator<Side, ExcludeQuiet>::generate() {
    this->generateAllPawnMoves();
    this->generateOffsetMoves<Bitboards::knight>(Piece::knight(Side));
    this->generateSlidingMoves<Bitboards::bishop>(Piece::bishop(Side));
    this->generateSlidingMoves<Bitboards::rook>(Piece::rook(Side));
    this->generateSlidingMoves<Bitboards::queen>(Piece::queen(Side));
    this->generateOffsetMoves<Bitboards::king>(Piece::king(Side));
}



template class MoveGenerator<Color::White, false>;
template class MoveGenerator<Color::Black, false>;
template class MoveGenerator<Color::White, true>;
template class MoveGenerator<Color::Black, true>;
