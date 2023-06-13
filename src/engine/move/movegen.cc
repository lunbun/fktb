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

    Bitboard pinned_;
    // Mobility of pieces. Pinned pieces have restricted mobility. If a piece is not pinned, will be set to Bitboards::All.
    SquareMap<Bitboard> mobility_;

    template<PieceType EnemySlider, Bitboard (*GenerateAttacksOnEmpty)(Square)>
    void calculatePins(Square king);

    // Note: These do not mask pinned piece mobility, the caller must do that.
    void serializeQuiet(Square from, Bitboard quiet);
    void serializeCaptures(Square from, Bitboard captures);
    // Serializes all moves in a bitboard, both quiet and captures (will not serialize quiet if in tactical mode)
    void serializeBitboard(Square from, Bitboard bitboard);

    void serializePromotion(Square from, Square to);
    void serializePromotionCapture(Square from, Square to);
    // Serializes an en passant move, only if it is legal (or if in pseudo-legal mode). En passant is not legal if it reveals a
    // check on the king.
    void maybeSerializeEnPassant(Square from, Square to);

    void generatePinnedPawnMoves(Square pawn);
    // Generates all pawn captures to either the left or right.
    template<bool Left>
    void generateAllPawnCapturesToSide(Bitboard forwardOne);
    void generateAllPawnMoves();

    void generateAllKnightMoves();

    void maybeGenerateCastlingMove(Square from, Square to, CastlingRights rights, MoveFlag flag, Bitboard empty, Bitboard check);
    void generateAllCastlingMoves();
    void generateKingMoves();

    template<PieceType Slider>
    void generateSlidingMoves();
};



template<Color Side, uint32_t Flags>
template<PieceType EnemySlider, Bitboard (*GenerateAttacksOnEmpty)(Square)>
INLINE void MoveGenerator<Side, Flags>::calculatePins(Square king) {
    static_assert(Flags & MoveGeneration::Flags::Legal, "Pinned pieces are only needed for legal move generation.");

    Bitboard enemySliders = this->board_.bitboard({ ~Side, EnemySlider });
    for (Square slider : enemySliders) {
        Bitboard between = Bitboards::between(king, slider);

        // Mask the between bitboard by the slider's attacks on an empty board, so that rooks cannot pin diagonally and bishops
        // cannot pin orthogonally.
        between &= GenerateAttacksOnEmpty(slider);

        // We have to mask all occupied pieces, not just friendly pieces because otherwise a slider could pin a friendly piece
        // through an enemy piece.
        //
        // If we calculate a pin on an enemy piece to our king, then it is actually a discovered check, not a pin. But it doesn't
        // matter since that entry in the pinned piece table won't ever be used, since we use the pinned piece table for
        // generating moves for friendly pieces.
        Bitboard piecesBetween = between & this->occupied_;

        if (piecesBetween.count() == 1) {
            // The piece is pinned (the piece might be an enemy piece, in which case it's a discovered check).
            Square pinned = Intrinsics::bsf(piecesBetween);

            this->pinned_.set(pinned);

            // Allow the slider to be captured by the pinned piece.
            between.set(slider);
            this->mobility_[pinned] &= between;
        }
    }
}

template<Color Side, uint32_t Flags>
INLINE MoveGenerator<Side, Flags>::MoveGenerator(Board &board, MoveEntry *moves) : board_(board), list_(moves) {
    constexpr Color Enemy = ~Side;

    this->friendly_ = board.composite(Side);
    this->enemy_ = board.composite(Enemy);
    this->occupied_ = this->friendly_ | this->enemy_;
    this->empty_ = ~this->occupied_;

    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        // These are not needed for pseudo-legal move generation.
        this->enemyAttacks_ = Bitboards::allAttacks<Enemy>(board, this->occupied_);

        this->pinned_ = Bitboards::Empty;
        // By default, all pieces have full mobility.
        this->mobility_.fill(Bitboards::All);

        // Calculate pinned pieces.
        Square king = board.king(Side);
        this->calculatePins<PieceType::Bishop, Bitboards::bishopAttacksOnEmpty>(king);
        this->calculatePins<PieceType::Rook, Bitboards::rookAttacksOnEmpty>(king);
        this->calculatePins<PieceType::Queen, Bitboards::queenAttacksOnEmpty>(king);
    }
}



template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializeQuiet(Square from, Bitboard quiet) {
    static_assert(Flags & MoveGeneration::Flags::Quiet, "Quiet moves are only needed for quiet move generation.");
    for (Square to : quiet) {
        this->list_.push({ from, to, MoveFlag::Quiet });
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializeCaptures(Square from, Bitboard captures) {
    static_assert(Flags & MoveGeneration::Flags::Tactical, "Captures are only needed for tactical move generation.");
    for (Square to : captures) {
        this->list_.push({ from, to, MoveFlag::Capture });
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializeBitboard(Square from, Bitboard bitboard) {
    if constexpr (Flags & MoveGeneration::Flags::Quiet) {
        this->serializeQuiet(from, bitboard & this->empty_);
    }
    if constexpr (Flags & MoveGeneration::Flags::Tactical) {
        this->serializeCaptures(from, bitboard & this->enemy_);
    }
}



// Returns the square shifted n ranks forward for the given side.
template<Color Side>
INLINE Square forwardRanks(Square square, uint8_t ranks) {
    if constexpr (Side == Color::White) {
        return square + (ranks * 8);
    } else {
        return square - (ranks * 8);
    }
}

// Returns the square shifted n ranks backward for the given side.
template<Color Side>
INLINE Square backwardRanks(Square square, uint8_t ranks) {
    if constexpr (Side == Color::White) {
        return square - (ranks * 8);
    } else {
        return square + (ranks * 8);
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializePromotion(Square from, Square to) {
    static_assert(Flags & MoveGeneration::Flags::Tactical, "Promotions are only needed for tactical move generation.");
    this->list_.push({ from, to, MoveFlag::KnightPromotion });
    this->list_.push({ from, to, MoveFlag::BishopPromotion });
    this->list_.push({ from, to, MoveFlag::RookPromotion });
    this->list_.push({ from, to, MoveFlag::QueenPromotion });
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::serializePromotionCapture(Square from, Square to) {
    static_assert(Flags & MoveGeneration::Flags::Tactical, "Promotions are only needed for tactical move generation.");
    this->list_.push({ from, to, MoveFlag::KnightPromoCapture });
    this->list_.push({ from, to, MoveFlag::BishopPromoCapture });
    this->list_.push({ from, to, MoveFlag::RookPromoCapture });
    this->list_.push({ from, to, MoveFlag::QueenPromoCapture });
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::maybeSerializeEnPassant(Square from, Square to) {
    Move move(from, to, MoveFlag::EnPassant);

    bool isLegal = true;

    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        // For en passant captures, don't do any fancy pin calculation, instead just make the move and see if it leaves the king
        // in check.
        Board &board = this->board_;

        // Board::isInCheck() only uses bitboards, so we can use MakeMoveType::BitboardsOnly.
        MakeMoveInfo info = board.makeMove<MakeMoveType::BitboardsOnly>(move);

        isLegal = !board.isInCheck<Side>();

        board.unmakeMove<MakeMoveType::BitboardsOnly>(move, info);
    }

    if (isLegal) {
        this->list_.push(move);
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::generatePinnedPawnMoves(Square pawn) {
    static_assert(Flags & MoveGeneration::Flags::Legal, "Pinned pieces are only in legal move generation.");

    constexpr Bitboard PromotionRank = (Side == Color::White) ? Bitboards::Rank8 : Bitboards::Rank1;
    constexpr Bitboard DoublePushRank = (Side == Color::White) ? Bitboards::Rank4 : Bitboards::Rank5;

    Bitboard bitboard = (1ULL << pawn);
    Bitboard mobility = this->mobility_[pawn];

    Bitboard singlePush = bitboard.shiftForward<Side>(1) & this->empty_ & mobility;

    // Promotions
    Bitboard promotion = singlePush & PromotionRank;
    if constexpr (Flags & MoveGeneration::Flags::Tactical) {
        if (promotion) {
            this->serializePromotion(pawn, forwardRanks<Side>(pawn, 1));
        }
    }

    // Single and double pawn pushes
    if constexpr (Flags & MoveGeneration::Flags::Quiet) {
        singlePush ^= promotion;

        if (singlePush) {
            this->list_.push({ pawn, forwardRanks<Side>(pawn, 1), MoveFlag::Quiet });

            Bitboard doublePush = singlePush.shiftForward<Side>(1) & this->empty_ & mobility & DoublePushRank;
            if (doublePush) {
                this->list_.push({ pawn, forwardRanks<Side>(pawn, 2), MoveFlag::DoublePawnPush });
            }
        }
    }

    // Captures
    if constexpr (Flags & MoveGeneration::Flags::Tactical) {
        Square enPassantSquare = this->board_.enPassantSquare();

        // Bitboard with pawn attacks, regardless of if there is an enemy piece there
        Bitboard captures = Bitboards::pawnAttacks<Side>(pawn) & mobility;
        if (enPassantSquare.isValid() && captures.get(enPassantSquare)) {
            this->maybeSerializeEnPassant(pawn, enPassantSquare);
        }

        // Mask out any captures that are not to an enemy piece
        captures &= this->enemy_;

        // Mask any promotion captures
        Bitboard promotionCaptures = captures & PromotionRank;

        // Remove promotion captures from captures
        captures ^= promotionCaptures;

        // Serialize captures and promotion captures
        this->serializeCaptures(pawn, captures);
        for (Square promoCapture : promotionCaptures) {
            this->serializePromotionCapture(pawn, promoCapture);
        }
    }
}

template<Color Side, uint32_t Flags>
template<bool Left>
INLINE void MoveGenerator<Side, Flags>::generateAllPawnCapturesToSide(Bitboard forwardOne) {
    static_assert(Flags & MoveGeneration::Flags::Tactical, "Captures are only needed for tactical move generation.");

    constexpr Bitboard PromotionRank = (Side == Color::White) ? Bitboards::Rank8 : Bitboards::Rank1;
    constexpr Bitboard CaptureFiles = Left ? (~Bitboards::FileA) : (~Bitboards::FileH);
    constexpr int8_t Offset = Left ? -1 : 1;

    Square enPassantSquare = this->board_.enPassantSquare();

    // Bitboard with all pawn attacks, regardless of if there is an enemy piece there
    Bitboard captures = (forwardOne & CaptureFiles);
    if constexpr (Left) {
        captures >>= 1;
    } else {
        captures <<= 1;
    }

    // Check for en passant captures
    if (enPassantSquare.isValid() && captures.get(enPassantSquare)) {
        Square from = backwardRanks<Side>(enPassantSquare, 1) - Offset;
        this->maybeSerializeEnPassant(from, enPassantSquare);
    }

    // Mask out any captures that are not to an enemy piece
    captures &= this->enemy_;

    // Mask any promotion captures
    Bitboard promoCaptures = captures & PromotionRank;

    // Remove promotion captures from captures
    captures ^= promoCaptures;

    // Serialize captures
    for (Square capture : captures) {
        Square from = backwardRanks<Side>(capture, 1) - Offset;
        this->list_.push({ from, capture, MoveFlag::Capture });
    }

    // Serialize promotion captures
    for (Square promoCapture : promoCaptures) {
        Square from = backwardRanks<Side>(promoCapture, 1) - Offset;
        this->serializePromotionCapture(from, promoCapture);
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::generateAllPawnMoves() {
    constexpr Bitboard PromotionRank = (Side == Color::White) ? Bitboards::Rank8 : Bitboards::Rank1;
    constexpr Bitboard DoublePushRank = (Side == Color::White) ? Bitboards::Rank4 : Bitboards::Rank5;

    Bitboard pawns = this->board_.bitboard(Piece::pawn(Side));

    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        // Pinned pawns are treated specially.
        Bitboard pinnedPawns = pawns & this->pinned_;
        for (Square pinnedPawn : pinnedPawns) {
            this->generatePinnedPawnMoves(pinnedPawn);
        }

        // Remove pinned pawns from the list of pawns.
        pawns ^= pinnedPawns;
    }

    Bitboard forwardOne = pawns.shiftForward<Side>(1);

    Bitboard singlePushes = forwardOne & this->empty_;

    // Promotions
    Bitboard promotions = singlePushes & PromotionRank;
    if constexpr (Flags & MoveGeneration::Flags::Tactical) {
        for (Square promotion : promotions) {
            Square from = backwardRanks<Side>(promotion, 1);
            this->serializePromotion(from, promotion);
        }
    }

    // Single and double pawn pushes
    if constexpr (Flags & MoveGeneration::Flags::Quiet) {
        singlePushes ^= promotions;
        for (Square singlePush : singlePushes) {
            this->list_.push({ backwardRanks<Side>(singlePush, 1), singlePush, MoveFlag::Quiet });
        }

        Bitboard doublePushes = singlePushes.shiftForward<Side>(1) & this->empty_ & DoublePushRank;
        for (Square doublePush : doublePushes) {
            this->list_.push({ backwardRanks<Side>(doublePush, 2), doublePush, MoveFlag::DoublePawnPush });
        }
    }

    // Captures
    if constexpr (Flags & MoveGeneration::Flags::Tactical) {
        this->generateAllPawnCapturesToSide<true>(forwardOne);
        this->generateAllPawnCapturesToSide<false>(forwardOne);
    }
}



template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::generateAllKnightMoves() {
    Bitboard pieces = this->board_.bitboard(Piece::knight(Side));
    for (Square square : pieces) {
        Bitboard attacks = Bitboards::knightAttacks(square);

        if constexpr (Flags & MoveGeneration::Flags::Legal) {
            // Mask pinned piece mobility
            attacks &= this->mobility_[square];
        }

        this->serializeBitboard(square, attacks);
    }
}



// Generates a castling move if it is legal and valid.
// Parameters:
//  - rights: Castling rights to check
//  - flag: Move flag to use if castling is valid
//  - empty: Squares that must be empty for the king and rook to see each other
//  - check: Squares that must not be attacked for the king to castle (since cannot castle out of, though, or into check)
template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::maybeGenerateCastlingMove(Square from, Square to, CastlingRights rights, MoveFlag flag,
    Bitboard empty, Bitboard check) {
    bool canCastle = (this->board_.castlingRights() & rights) && !(this->occupied_ & empty);

    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        canCastle = canCastle && !(this->enemyAttacks_ & check);
    }

    if (canCastle) {
        this->list_.push({ from, to, flag });
    }
}

template<Color Side, uint32_t Flags>
INLINE void MoveGenerator<Side, Flags>::generateAllCastlingMoves() {
    static_assert((Flags & MoveGeneration::Flags::Quiet) && !(Flags & MoveGeneration::Flags::Evasion));

    if constexpr (Side == Color::White) {
        constexpr Bitboard KingSideEmpty = Bitboards::F1 | Bitboards::G1;
        constexpr Bitboard QueenSideEmpty = Bitboards::B1 | Bitboards::C1 | Bitboards::D1;
        constexpr Bitboard KingSideCheck = Bitboards::E1 | Bitboards::F1 | Bitboards::G1;
        constexpr Bitboard QueenSideCheck = Bitboards::C1 | Bitboards::D1 | Bitboards::E1;

        this->maybeGenerateCastlingMove(Square::E1, Square::G1, CastlingRights::WhiteKingSide, MoveFlag::KingCastle,
            KingSideEmpty, KingSideCheck);
        this->maybeGenerateCastlingMove(Square::E1, Square::C1, CastlingRights::WhiteQueenSide, MoveFlag::QueenCastle,
            QueenSideEmpty, QueenSideCheck);
    } else {
        constexpr Bitboard KingSideEmpty = Bitboards::F8 | Bitboards::G8;
        constexpr Bitboard QueenSideEmpty = Bitboards::B8 | Bitboards::C8 | Bitboards::D8;
        constexpr Bitboard KingSideCheck = Bitboards::E8 | Bitboards::F8 | Bitboards::G8;
        constexpr Bitboard QueenSideCheck = Bitboards::C8 | Bitboards::D8 | Bitboards::E8;

        this->maybeGenerateCastlingMove(Square::E8, Square::G8, CastlingRights::BlackKingSide, MoveFlag::KingCastle,
            KingSideEmpty, KingSideCheck);
        this->maybeGenerateCastlingMove(Square::E8, Square::C8, CastlingRights::BlackQueenSide, MoveFlag::QueenCastle,
            QueenSideEmpty, QueenSideCheck);
    }
}

template<Color Side, uint32_t Flags>
void MoveGenerator<Side, Flags>::generateKingMoves() {
    // Normal king moves
    Square king = this->board_.king(Side);
    Bitboard attacks = Bitboards::kingAttacks(king);

    if constexpr (Flags & MoveGeneration::Flags::Legal) {
        // Do not allow king to move into check
        attacks &= ~this->enemyAttacks_;
    }

    this->serializeBitboard(king, attacks);

    // Castling moves
    if constexpr ((Flags & MoveGeneration::Flags::Quiet) && !(Flags & MoveGeneration::Flags::Evasion)) {
        this->generateAllCastlingMoves();
    }
}



template<Color Side, uint32_t Flags>
template<PieceType Slider>
INLINE void MoveGenerator<Side, Flags>::generateSlidingMoves() {
    Bitboard pieces = this->board_.bitboard({ Side, Slider });
    for (Square square : pieces) {
        Bitboard attacks = Bitboards::sliderAttacks(Slider, square, this->occupied_);

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
    this->generateAllKnightMoves();
    this->generateSlidingMoves<PieceType::Bishop>();
    this->generateSlidingMoves<PieceType::Rook>();
    this->generateSlidingMoves<PieceType::Queen>();
    this->generateKingMoves();

    return this->list_.pointer();
}



template<Color Side>
INLINE MoveEntry *filterLegal(Board &board, MoveEntry *start, MoveEntry *end) {
    MoveEntry *result = start;
    // TODO: This is a very inefficient method. Implement a more efficient method.
    for (MoveEntry *entry = start; entry != end; entry++) {
        Move move = entry->move;

        // Board::isInCheck() only uses bitboards, so we can use MakeMoveType::BitboardsOnly.
        MakeMoveInfo info = board.makeMove<MakeMoveType::BitboardsOnly>(move);

        // TODO: Do not allow castling through check (or in check)
        if (!board.isInCheck<Side>()) {
            *(result++) = *entry;
        }

        board.unmakeMove<MakeMoveType::BitboardsOnly>(move, info);
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

            // For now, just have the evasion generator generate all moves and then filter them by legality
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
template MoveEntry *MoveGeneration::generate<Color::White, MoveGeneration::Type::Quiet>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::Black, MoveGeneration::Type::Quiet>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::White, MoveGeneration::Type::Tactical>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::Black, MoveGeneration::Type::Tactical>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::White, MoveGeneration::Type::Legal>(Board &, MoveEntry *);
template MoveEntry *MoveGeneration::generate<Color::Black, MoveGeneration::Type::Legal>(Board &, MoveEntry *);
