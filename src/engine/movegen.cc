#include "movegen.h"

#include "bitboard.h"

template<Color Side>
MoveGenerator<Side>::MoveGenerator(const Board &board, MovePriorityQueueStack &moves) : board_(board), moves_(moves) {
    this->friendly_ = board.composite<Side>();
    this->enemy_ = board.composite<~Side>();
    this->occupied_ = this->friendly_ | this->enemy_;
    this->empty_ = ~this->occupied_;
}

template<Color Side>
void MoveGenerator<Side>::serializeBitboard(Square from, Bitboard bitboard) {
    while (bitboard) {
        Square to = bitboard.bsfReset();
        this->moves_.enqueue({ from, to, MoveFlag::Quiet });
    }
}

template<Color Side>
void MoveGenerator<Side>::generatePawnMoves(Square square) {
    Bitboard empty = this->empty_;

    if constexpr (Side == Color::White) {
        Bitboard singlePush = ((1ULL << square) << 8) & empty;
        this->serializeBitboard(square, singlePush);

        Bitboard doublePush = (singlePush << 8) & empty & Bitboards::Rank4;
        this->serializeBitboard(square, doublePush);
    } else {
        Bitboard singlePush = ((1ULL << square) >> 8) & empty;
        this->serializeBitboard(square, singlePush);

        Bitboard doublePush = (singlePush >> 8) & empty & Bitboards::Rank5;
        this->serializeBitboard(square, doublePush);
    }

    Bitboard attacks = Bitboards::pawn<Side>(square) & this->enemy_;
    this->serializeBitboard(square, attacks);
}

template<Color Side>
void MoveGenerator<Side>::generateAllPawnMoves() {
    Bitboard pawns = this->board_.template bitboard<Side>(PieceType::Pawn);
    while (pawns) {
        Square square = pawns.bsfReset();
        this->generatePawnMoves(square);
    }
}

template<Color Side>
template<Bitboard (*GenerateAttacks)(Square)>
void MoveGenerator<Side>::generateOffsetMoves(Piece piece) {
    Bitboard pieces = this->board_.template bitboard<Side>(piece.type());
    while (pieces) {
        Square square = pieces.bsfReset();

        Bitboard attacks = GenerateAttacks(square) & ~this->friendly_;
        this->serializeBitboard(square, attacks);
    }
}

template<Color Side>
template<Bitboard (*GenerateAttacks)(Square, Bitboard)>
void MoveGenerator<Side>::generateSlidingMoves(Piece piece) {
    Bitboard pieces = this->board_.template bitboard<Side>(piece.type());
    while (pieces) {
        Square square = pieces.bsfReset();

        Bitboard attacks = GenerateAttacks(square, this->occupied_) & ~this->friendly_;
        this->serializeBitboard(square, attacks);
    }
}



template<Color Side>
void MoveGenerator<Side>::generate() {
    this->generateAllPawnMoves();
    this->generateOffsetMoves<Bitboards::knight>(Piece::knight(Side));
    this->generateSlidingMoves<Bitboards::bishop>(Piece::bishop(Side));
    this->generateSlidingMoves<Bitboards::rook>(Piece::rook(Side));
    this->generateSlidingMoves<Bitboards::queen>(Piece::queen(Side));
    this->generateOffsetMoves<Bitboards::king>(Piece::king(Side));
}



template class MoveGenerator<Color::White>;
template class MoveGenerator<Color::Black>;
