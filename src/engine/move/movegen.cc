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
template<Color Side, uint32_t Flags>
class MoveGenerator {
public:
    MoveGenerator() = delete;
    MoveGenerator(Board &board, MoveEntry *moves);

    // Returns the pointer to the end of the move list.
    [[nodiscard]] MoveEntry *generate();

private:
    Board &board_;
    MoveList list_;

    Bitboard friendly_;
    Bitboard enemy_;
    Bitboard occupied_;
    Bitboard empty_;

    Bitboard enemyAttacks_;

    // Mobility of pieces. Pinned pieces have restricted mobility. If a piece is not pinned, will be set to Bitboards::All.
    SquareMap<Bitboard> mobility_;

    template<PieceType EnemySlider>
    void calculatePins(Square king);

    // Note: These do not mask pinned piece mobility, the caller must do that.
    void serializeQuiet(Square from, Bitboard quiet);
    void serializeCaptures(Square from, Bitboard captures);
    void serializePromotions(Square from, Bitboard promotions);
    void serializePromotionCaptures(Square from, Bitboard promoCaptures);
    // Serializes all moves in a bitboard, both quiet and captures (will not serialize quiet if in tactical mode)
    void serializeBitboard(Square from, Bitboard bitboard);

    void generatePawnMoves(Square square);
    void generateAllPawnMoves();

    void generateKingMoves();

    template<Bitboard (*GenerateAttacks)(Square)>
    void generateOffsetMoves(Piece piece);

    template<Bitboard (*GenerateAttacks)(Square, Bitboard)>
    void generateSlidingMoves(Piece piece);
};



template<Color Side, uint32_t Flags>
template<PieceType EnemySlider>
INLINE void MoveGenerator<Side, Flags>::calculatePins(Square king) {
    static_assert(Flags & MoveGeneration::Flags::Legal, "Pinned pieces are only needed for legal move generation.");

    Bitboard enemySliders = this->board_.template bitboard<~Side>(EnemySlider);
    for (Square slider : enemySliders) {
        Bitboard between = Bitboards::between(king, slider);

        // Mask the between bitboard by the slider's attacks on an empty board, so that rooks cannot pin diagonally and bishops
        // cannot pin orthogonally.
        between &= Bitboards::sliderAttacksOnEmpty<EnemySlider>(slider);

        Bitboard friendlyBetween = between & this->friendly_;

        if (friendlyBetween.count() == 1) {
            // The friendly piece is pinned.
            Square pinned = Intrinsics::bsf(friendlyBetween);

            // Allow the slider to be captured by the pinned piece.
            between.set(slider);
            this->mobility_[pinned] &= between;
        }
    }
}

template<Color Side, uint32_t Flags>
MoveGenerator<Side, Flags>::MoveGenerator(Board &board, MoveEntry *moves) : board_(board), list_(moves) {
    constexpr Color Enemy = ~Side;

    this->friendly_ = board.composite<Side>();
    this->enemy_ = board.composite<Enemy>();
    this->occupied_ = this->friendly_ | this->enemy_;
    this->empty_ = ~this->occupied_;

    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        // These are not needed for pseudo-legal move generation.
        this->enemyAttacks_ = Bitboards::allAttacks<Enemy>(board, this->occupied_);

        // Calculate mobility for pinned pieces.

        // By default, all pieces have full mobility.
        this->mobility_.fill(Bitboards::All);

        // Calculate pinned pieces.
        Square king = board.king<Side>();
        this->calculatePins<PieceType::Bishop>(king);
        this->calculatePins<PieceType::Rook>(king);
        this->calculatePins<PieceType::Queen>(king);
    }
}



template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializeQuiet(Square from, Bitboard quiet) {
    static_assert(!(Flags & MoveGeneration::Flags::Tactical), "Cannot serialize quiet moves in tactical move generation.");

    for (Square to : quiet) {
        this->list_.push({ from, to, MoveFlag::Quiet });
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializeCaptures(Square from, Bitboard captures) {
    for (Square to : captures) {
        this->list_.push({ from, to, MoveFlag::Capture });
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializePromotions(Square from, Bitboard promotions) {
    for (Square to : promotions) {
        this->list_.push({ from, to, MoveFlag::KnightPromotion });
        this->list_.push({ from, to, MoveFlag::BishopPromotion });
        this->list_.push({ from, to, MoveFlag::RookPromotion });
        this->list_.push({ from, to, MoveFlag::QueenPromotion });
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializePromotionCaptures(Square from, Bitboard promoCaptures) {
    for (Square to : promoCaptures) {
        this->list_.push({ from, to, MoveFlag::KnightPromoCapture });
        this->list_.push({ from, to, MoveFlag::BishopPromoCapture });
        this->list_.push({ from, to, MoveFlag::RookPromoCapture });
        this->list_.push({ from, to, MoveFlag::QueenPromoCapture });
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializeBitboard(Square from, Bitboard bitboard) {
    if constexpr (!(Flags & MoveGeneration::Flags::Tactical)) {
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

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::generatePawnMoves(Square square) {
    constexpr Bitboard PromotionRank = (Side == Color::White) ? Bitboards::Rank8 : Bitboards::Rank1;
    constexpr Bitboard DoublePushToRank = (Side == Color::White) ? Bitboards::Rank4 : Bitboards::Rank5;

    Bitboard empty = this->empty_;

    // Pawn pushes
    // Single push (quiet, but needed to check for promotions, which are not quiet)
    Bitboard singlePush = forwardOneRank<Side>(1ULL << square) & empty;

    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        // Mask pawns that are pinned.
        singlePush &= this->mobility_[square];
    }

    Bitboard promotions = singlePush & PromotionRank;

    this->serializePromotions(square, promotions);

    if constexpr (!(Flags & MoveGeneration::Flags::Tactical)) {
        this->serializeQuiet(square, singlePush ^ promotions);

        // Double push (always quiet)
        Bitboard doublePush = forwardOneRank<Side>(singlePush) & empty & DoublePushToRank;
        this->serializeQuiet(square, doublePush);
    }

    // Pawn captures
    Bitboard attacks = Bitboards::pawnAttacks<Side>(square) & this->enemy_;

    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        // Mask pawns that are pinned.
        attacks &= this->mobility_[square];
    }

    Bitboard promotionsCaptures = attacks & PromotionRank;
    Bitboard captures = attacks ^ promotionsCaptures;
    this->serializePromotionCaptures(square, promotionsCaptures);
    this->serializeCaptures(square, captures);
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::generateAllPawnMoves() {
    Bitboard pawns = this->board_.template bitboard<Side>(PieceType::Pawn);
    for (Square square : pawns) {
        this->generatePawnMoves(square);
    }
}

template<Color Side, uint32_t Flags>
void MoveGenerator<Side, Flags>::generateKingMoves() {
    // Normal king moves
    Square king = this->board_.template king<Side>();
    Bitboard attacks = Bitboards::kingAttacks(king);

    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        // Do not allow king to move into check
        attacks &= ~this->enemyAttacks_;
    }

    this->serializeBitboard(king, attacks);

    // Castling moves
    if constexpr (!(Flags & MoveGeneration::Flags::Tactical) && !(Flags & MoveGeneration::Flags::Evasion)) {
        // From and to squares for castling
        constexpr Square KingFrom = (Side == Color::White) ? Square::E1 : Square::E8;
        constexpr Square KingSideTo = (Side == Color::White) ? Square::G1 : Square::G8;
        constexpr Square QueenSideTo = (Side == Color::White) ? Square::C1 : Square::C8;

        // Castling rights bits
        constexpr CastlingRights KingSide = (Side == Color::White)
            ? CastlingRights::WhiteKingSide
            : CastlingRights::BlackKingSide;
        constexpr CastlingRights QueenSide = (Side == Color::White)
            ? CastlingRights::WhiteQueenSide
            : CastlingRights::BlackQueenSide;

        // Squares that must be empty to castle (i.e. rook is visible to king)
        constexpr Bitboard KingSideVisibility = (Side == Color::White)
            ? Bitboards::F1 | Bitboards::G1
            : Bitboards::F8 | Bitboards::G8;
        constexpr Bitboard QueenSideVisibility = (Side == Color::White)
            ? Bitboards::B1 | Bitboards::C1 | Bitboards::D1
            : Bitboards::B8 | Bitboards::C8 | Bitboards::D8;

        // Squares that must not be attacked to castle (no castling through, out of, or into check)
        constexpr Bitboard KingSideCheck = (Side == Color::White)
            ? Bitboards::E1 | Bitboards::F1 | Bitboards::G1
            : Bitboards::E8 | Bitboards::F8 | Bitboards::G8;
        constexpr Bitboard QueenSideCheck = (Side == Color::White)
            ? Bitboards::E1 | Bitboards::D1 | Bitboards::C1
            : Bitboards::E8 | Bitboards::D8 | Bitboards::C8;

        // King-side castling
        {
            bool canCastle = (this->board_.castlingRights() & KingSide) && !(this->occupied_ & KingSideVisibility);

            if constexpr (Flags & MoveGeneration::Flags::Legal) {
                canCastle = canCastle && !(this->enemyAttacks_ & KingSideCheck);
            }

            if (canCastle) {
                this->list_.push({ KingFrom, KingSideTo, MoveFlag::KingCastle });
            }
        }

        // Queen-side castling
        {
            bool canCastle = (this->board_.castlingRights() & QueenSide) && !(this->occupied_ & QueenSideVisibility);

            if constexpr (Flags & MoveGeneration::Flags::Legal) {
                canCastle = canCastle && !(this->enemyAttacks_ & QueenSideCheck);
            }

            if (canCastle) {
                this->list_.push({ KingFrom, QueenSideTo, MoveFlag::QueenCastle });
            }
        }
    }
}

template<Color Side, uint32_t Flags>
template<Bitboard (*GenerateAttacks)(Square)>
INLINE void MoveGenerator<Side, Flags>::generateOffsetMoves(Piece piece) {
    Bitboard pieces = this->board_.template bitboard<Side>(piece.type());
    for (Square square : pieces) {
        Bitboard attacks = GenerateAttacks(square);

        if constexpr (Flags & MoveGeneration::Flags::Legal) {
            // Mask pinned piece mobility
            attacks &= this->mobility_[square];
        }

        this->serializeBitboard(square, attacks);
    }
}

template<Color Side, uint32_t Flags>
template<Bitboard (*GenerateAttacks)(Square, Bitboard)>
INLINE void MoveGenerator<Side, Flags>::generateSlidingMoves(Piece piece) {
    Bitboard pieces = this->board_.template bitboard<Side>(piece.type());
    for (Square square : pieces) {
        Bitboard attacks = GenerateAttacks(square, this->occupied_);

        if constexpr (Flags & MoveGeneration::Flags::Legal) {
            // Mask pinned piece mobility
            attacks &= this->mobility_[square];
        }

        this->serializeBitboard(square, attacks);
    }
}

template<Color Side, uint32_t Flags>
INLINE MoveEntry *MoveGenerator<Side, Flags>::generate() {
    this->generateAllPawnMoves();
    this->generateOffsetMoves<Bitboards::knightAttacks>(Piece::knight(Side));
    this->generateSlidingMoves<Bitboards::bishopAttacks>(Piece::bishop(Side));
    this->generateSlidingMoves<Bitboards::rookAttacks>(Piece::rook(Side));
    this->generateSlidingMoves<Bitboards::queenAttacks>(Piece::queen(Side));
    this->generateKingMoves();

    return this->list_.pointer();
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

template<Color Side, uint32_t Flags>
MoveEntry *MoveGeneration::generate(Board &board, MoveEntry *moves) {
    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        // If we are in check, switch to the evasion generator
        if (board.isInCheck<Side>()) {
            constexpr uint32_t EvasionFlags = Flags | MoveGeneration::Flags::Evasion;

            MoveGenerator<Side, EvasionFlags> generator(board, moves);
            MoveEntry *end = generator.generate();

            // For now, just have the evasion generator generate all moves and then filter them
            // TODO: In the future, have specialized generation code for evasion generator
            return filterLegal<Side>(board, moves, end);
        }
    }

    MoveGenerator<Side, Flags> generator(board, moves);
    return generator.generate();
}

RootMoveList MoveGeneration::generateLegalRoot(Board &board) {
    AlignedMoveEntry movesBuffer[MaxMoveCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(movesBuffer);

    MoveEntry *movesEnd;
    if (board.turn() == Color::White) {
        movesEnd = MoveGeneration::generate<Color::White, MoveGeneration::Type::Legal>(board, movesStart);
    } else {
        movesEnd = MoveGeneration::generate<Color::Black, MoveGeneration::Type::Legal>(board, movesStart);
    }

    // RootMoveList will copy the moves into its own buffer
    return { movesStart, movesEnd };
}

template MoveEntry *MoveGeneration::generate<Color::White, MoveGeneration::Type::PseudoLegal>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::Black, MoveGeneration::Type::PseudoLegal>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::White, MoveGeneration::Type::Tactical>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::Black, MoveGeneration::Type::Tactical>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::White, MoveGeneration::Type::Legal>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::Black, MoveGeneration::Type::Legal>(Board &, MoveEntry *);
