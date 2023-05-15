#include "movegen.h"

// Generates all legal diagonal moves from a square.
inline void generateDiagonalMoves(Piece piece, const Board &board, MovePriorityQueue &moves) {
    // Generate moves to the top-right.
    {
        auto file = static_cast<int8_t>(piece.file() + 1);
        auto rank = static_cast<int8_t>(piece.rank() + 1);
        while (file < 8 && rank < 8) {
            Square to = { file, rank };
            Piece *pieceAtTo = board.getPieceAt(to);
            if (pieceAtTo == nullptr) { // Move to an empty square
                moves.append(to, piece);
            } else if (pieceAtTo->color() != piece.color()) { // Capture an enemy piece
                moves.append(to, piece, *pieceAtTo);
                break;
            } else { // Blocked by a friendly piece
                break;
            }

            ++file;
            ++rank;
        }
    }

    // Generate moves to the bottom-right.
    {
        auto file = static_cast<int8_t>(piece.file() + 1);
        auto rank = static_cast<int8_t>(piece.rank() - 1);
        while (file < 8 && rank >= 0) {
            Square to = { file, rank };
            Piece *pieceAtTo = board.getPieceAt(to);
            if (pieceAtTo == nullptr) { // Move to an empty square
                moves.append(to, piece);
            } else if (pieceAtTo->color() != piece.color()) { // Capture an enemy piece
                moves.append(to, piece, *pieceAtTo);
                break;
            } else { // Blocked by a friendly piece
                break;
            }

            ++file;
            --rank;
        }
    }

    // Generate moves to the bottom-left.
    {
        auto file = static_cast<int8_t>(piece.file() - 1);
        auto rank = static_cast<int8_t>(piece.rank() - 1);
        while (file >= 0 && rank >= 0) {
            Square to = { file, rank };
            Piece *pieceAtTo = board.getPieceAt(to);
            if (pieceAtTo == nullptr) { // Move to an empty square
                moves.append(to, piece);
            } else if (pieceAtTo->color() != piece.color()) { // Capture an enemy piece
                moves.append(to, piece, *pieceAtTo);
                break;
            } else { // Blocked by a friendly piece
                break;
            }

            --file;
            --rank;
        }
    }

    // Generate moves to the top-left.
    {
        auto file = static_cast<int8_t>(piece.file() - 1);
        auto rank = static_cast<int8_t>(piece.rank() + 1);
        while (file >= 0 && rank < 8) {
            Square to = { file, rank };
            Piece *pieceAtTo = board.getPieceAt(to);
            if (pieceAtTo == nullptr) { // Move to an empty square
                moves.append(to, piece);
            } else if (pieceAtTo->color() != piece.color()) { // Capture an enemy piece
                moves.append(to, piece, *pieceAtTo);
                break;
            } else { // Blocked by a friendly piece
                break;
            }

            --file;
            ++rank;
        }
    }
}

// Generates all legal orthogonal moves from a square.
inline void generateOrthogonalMoves(Piece piece, const Board &board, MovePriorityQueue &moves) {
    // Generate moves to the right.
    for (auto file = static_cast<int8_t>(piece.file() + 1); file < 8; ++file) {
        Piece *pieceAtTo = board.getPieceAt({ file, piece.rank() });
        if (pieceAtTo == nullptr) { // Move to an empty square
            moves.append({ file, piece.rank() }, piece);
        } else if (pieceAtTo->color() != piece.color()) { // Capture an enemy piece
            moves.append({ file, piece.rank() }, piece, *pieceAtTo);
            break;
        } else { // Blocked by a friendly piece
            break;
        }
    }

    // Generate moves to the left.
    for (auto file = static_cast<int8_t>(piece.file() - 1); file >= 0; --file) {
        Piece *pieceAtTo = board.getPieceAt({ file, piece.rank() });
        if (pieceAtTo == nullptr) { // Move to an empty square
            moves.append({ file, piece.rank() }, piece);
        } else if (pieceAtTo->color() != piece.color()) { // Capture an enemy piece
            moves.append({ file, piece.rank() }, piece, *pieceAtTo);
            break;
        } else { // Blocked by a friendly piece
            break;
        }
    }

    // Generate moves to the top.
    for (auto rank = static_cast<int8_t>(piece.rank() + 1); rank < 8; ++rank) {
        Piece *pieceAtTo = board.getPieceAt({ piece.file(), rank });
        if (pieceAtTo == nullptr) { // Move to an empty square
            moves.append({ piece.file(), rank }, piece);
        } else if (pieceAtTo->color() != piece.color()) { // Capture an enemy piece
            moves.append({ piece.file(), rank }, piece, *pieceAtTo);
            break;
        } else { // Blocked by a friendly piece
            break;
        }
    }

    // Generate moves to the bottom.
    for (auto rank = static_cast<int8_t>(piece.rank() - 1); rank >= 0; --rank) {
        Piece *pieceAtTo = board.getPieceAt({ piece.file(), rank });
        if (pieceAtTo == nullptr) { // Move to an empty square
            moves.append({ piece.file(), rank }, piece);
        } else if (pieceAtTo->color() != piece.color()) { // Capture an enemy piece
            moves.append({ piece.file(), rank }, piece, *pieceAtTo);
            break;
        } else { // Blocked by a friendly piece
            break;
        }
    }
}



// Generates a forward pawn move if the destination square is empty. Returns true if a move was generated (i.e. the
// square was empty).
inline bool maybeGeneratePawnForwardMove(Square to, Piece piece, int8_t promotionRank, const Board &board,
    MovePriorityQueue &moves) {
    if (!to.isInBounds()) {
        return false;
    }

    Piece *pieceAtTo = board.getPieceAt(to);
    if (pieceAtTo == nullptr) {
        if (to.rank == promotionRank) {
//            moves.append(Move(to, piece, std::nullopt, PieceType::Queen));
//            moves.append(Move(to, piece, std::nullopt, PieceType::Rook));
//            moves.append(Move(to, piece, std::nullopt, PieceType::Bishop));
//            moves.append(Move(to, piece, std::nullopt, PieceType::Knight));
        } else {
            moves.append(to, piece);
        }
        return true;
    }

    return false;
}

// Generates a pawn capture move if the destination square is occupied by an enemy piece.
inline void maybeGeneratePawnCaptureMove(Square to, Piece piece, int8_t promotionRank, const Board &board,
    MovePriorityQueue &moves) {
    if (!to.isInBounds()) {
        return;
    }

    Piece *pieceAtTo = board.getPieceAt(to);
    if (pieceAtTo != nullptr && pieceAtTo->color() != piece.color()) {
        if (to.rank == promotionRank) {
//            moves.append(Move(to, piece, *pieceAtTo, PieceType::Queen));
//            moves.append(Move(to, piece, *pieceAtTo, PieceType::Rook));
//            moves.append(Move(to, piece, *pieceAtTo, PieceType::Bishop));
//            moves.append(Move(to, piece, *pieceAtTo, PieceType::Knight));
        } else {
            moves.append(to, piece, *pieceAtTo);
        }
    }
}

// Generates all legal pawn moves from a square.
inline void generatePawnMoves(Piece piece, const Board &board, MovePriorityQueue &moves) {
    int8_t direction = getPawnDirection(piece.color());
    auto promotionRank = static_cast<int8_t>(piece.color() == PieceColor::White ? 7 : 0);

    // Pawn moving forward one square.
    if (maybeGeneratePawnForwardMove(piece.square().offset(0, direction), piece, promotionRank, board, moves)) {
        // If we got here, we know that the square in front of the pawn is empty. So also check if we can move forward
        // two squares.
        auto startingRank = static_cast<int8_t>(piece.color() == PieceColor::White ? 1 : 6);
        if (piece.rank() == startingRank) {
            maybeGeneratePawnForwardMove(piece.square().offset(0, static_cast<int8_t>(direction * 2)), piece,
                promotionRank, board, moves);
        }
    }

    // Pawn captures
    maybeGeneratePawnCaptureMove(piece.square().offset(-1, direction), piece, promotionRank, board, moves);
    maybeGeneratePawnCaptureMove(piece.square().offset(1, direction), piece, promotionRank, board, moves);
}



// Generates a move to a square if the square is in bounds, and is empty or contains an enemy piece.
inline void maybeGenerateKnightMoveToSquare(Square to, Piece piece, const Board &board, MovePriorityQueue &moves) {
    if (!to.isInBounds()) {
        return;
    }

    Piece *pieceAtTo = board.getPieceAt(to);
    if (pieceAtTo == nullptr) {
        moves.append(to, piece);
    } else if (pieceAtTo->color() != piece.color()) {
        moves.append(to, piece, *pieceAtTo);
    }
}

// Generates all legal knight moves from a square.
inline void generateKnightMoves(Piece piece, const Board &board, MovePriorityQueue &moves) {
    maybeGenerateKnightMoveToSquare(piece.square().offset(1, 2), piece, board, moves);
    maybeGenerateKnightMoveToSquare(piece.square().offset(2, 1), piece, board, moves);
    maybeGenerateKnightMoveToSquare(piece.square().offset(2, -1), piece, board, moves);
    maybeGenerateKnightMoveToSquare(piece.square().offset(1, -2), piece, board, moves);
    maybeGenerateKnightMoveToSquare(piece.square().offset(-1, -2), piece, board, moves);
    maybeGenerateKnightMoveToSquare(piece.square().offset(-2, -1), piece, board, moves);
    maybeGenerateKnightMoveToSquare(piece.square().offset(-2, 1), piece, board, moves);
    maybeGenerateKnightMoveToSquare(piece.square().offset(-1, 2), piece, board, moves);
}



// Generates all legal bishop moves from a square.
inline void generateBishopMoves(Piece piece, const Board &board, MovePriorityQueue &moves) {
    generateDiagonalMoves(piece, board, moves);
}



// Generates all legal rook moves from a square.
inline void generateRookMoves(Piece piece, const Board &board, MovePriorityQueue &moves) {
    generateOrthogonalMoves(piece, board, moves);
}



// Generates all legal queen moves from a square.
inline void generateQueenMoves(Piece piece, const Board &board, MovePriorityQueue &moves) {
    generateDiagonalMoves(piece, board, moves);
    generateOrthogonalMoves(piece, board, moves);
}



// Generates a king move to a square if it is legal.
inline void maybeGenerateKingMoveToSquare(Square to, Piece piece, const Board &board, MovePriorityQueue &moves) {
    if (!to.isInBounds()) {
        return;
    }

    Piece *pieceAtTo = board.getPieceAt(to);
    if (pieceAtTo == nullptr) {
        moves.append(to, piece);
    } else if (pieceAtTo->color() != piece.color()) {
        moves.append(to, piece, *pieceAtTo);
    }
}

// Generates all legal king moves from a square.
inline void generateKingMoves(Piece piece, const Board &board, MovePriorityQueue &moves) {
    maybeGenerateKingMoveToSquare(piece.square().offset(1, 0), piece, board, moves);
    maybeGenerateKingMoveToSquare(piece.square().offset(1, 1), piece, board, moves);
    maybeGenerateKingMoveToSquare(piece.square().offset(0, 1), piece, board, moves);
    maybeGenerateKingMoveToSquare(piece.square().offset(-1, 1), piece, board, moves);
    maybeGenerateKingMoveToSquare(piece.square().offset(-1, 0), piece, board, moves);
    maybeGenerateKingMoveToSquare(piece.square().offset(-1, -1), piece, board, moves);
    maybeGenerateKingMoveToSquare(piece.square().offset(0, -1), piece, board, moves);
    maybeGenerateKingMoveToSquare(piece.square().offset(1, -1), piece, board, moves);
}



void MoveGenerator::generate(const Board &board, MovePriorityQueue &moves) {
    for (const auto &pawn : board.pawns()[board.turn()]) {
        generatePawnMoves(*pawn, board, moves);
    }

    for (const auto &knight : board.knights()[board.turn()]) {
        generateKnightMoves(*knight, board, moves);
    }

    for (const auto &bishop : board.bishops()[board.turn()]) {
        generateBishopMoves(*bishop, board, moves);
    }

    for (const auto &rook : board.rooks()[board.turn()]) {
        generateRookMoves(*rook, board, moves);
    }

    for (const auto &queen : board.queens()[board.turn()]) {
        generateQueenMoves(*queen, board, moves);
    }

    for (const auto &king : board.kings()[board.turn()]) {
        generateKingMoves(*king, board, moves);
    }
}
