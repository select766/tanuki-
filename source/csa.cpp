#include "csa.h"

#include <map>

#include "position.h"
#include "types.h"

namespace {
	File charCSAToFile(const char c) { return static_cast<File>(c - '1'); }
	Rank charCSAToRank(const char c) { return static_cast<Rank>(c - '1'); }

	class StringToPieceTypeCSA : public std::map<std::string, PieceType> {
	public:
		StringToPieceTypeCSA() {
			(*this)["FU"] = PAWN;
			(*this)["KY"] = LANCE;
			(*this)["KE"] = KNIGHT;
			(*this)["GI"] = SILVER;
			(*this)["KA"] = BISHOP;
			(*this)["HI"] = ROOK;
			(*this)["KI"] = GOLD;
			(*this)["OU"] = KING;
			(*this)["TO"] = PRO_PAWN;
			(*this)["NY"] = PRO_LANCE;
			(*this)["NK"] = PRO_KNIGHT;
			(*this)["NG"] = PRO_SILVER;
			(*this)["UM"] = HORSE;
			(*this)["RY"] = DRAGON;
		}
		PieceType value(const std::string& str) const {
			return this->find(str)->second;
		}
		bool isLegalString(const std::string& str) const {
			return (this->find(str) != this->end());
		}
	};
	const StringToPieceTypeCSA g_stringToPieceTypeCSA;
}

Move CSA::to_move(Position& pos, const std::string& str)
{
	if (str.size() != 6) {
		return Move::MOVE_NONE;
	}
	File toFile = charCSAToFile(str[2]);
	Rank toRank = charCSAToRank(str[3]);
	if (!is_ok(toFile) || !is_ok(toRank)) {
		return Move::MOVE_NONE;
	}
	Square to = toFile | toRank;
	std::string ptToString(str.begin() + 4, str.end());
	if (!g_stringToPieceTypeCSA.isLegalString(ptToString)) {
		return Move::MOVE_NONE;
	}
	PieceType ptTo = g_stringToPieceTypeCSA.value(ptToString);
	Move move;
	if (str[0] == '0' && str[1] == '0') {
		// drop
		move = make_move_drop(ptTo, to, pos.side_to_move());
	}
	else {
		File fromFile = charCSAToFile(str[0]);
		Rank fromRank = charCSAToRank(str[1]);
		if (!is_ok(fromFile) || !is_ok(fromRank)) {
			return Move::MOVE_NONE;
		}
		Square from = fromFile | fromRank;
		PieceType ptFrom = type_of(pos.piece_on(from));
		if (ptFrom == ptTo) {
			// non promote
			move = make_move(from, to, pos.side_to_move(), ptFrom);
		}
		else if (ptFrom + PIECE_TYPE_PROMOTE == ptTo) {
			// promote
			move = make_move_promote(from, to, pos.side_to_move(), ptFrom);
		}
		else {
			return Move::MOVE_NONE;
		}
	}

	if (pos.pseudo_legal(move) && pos.legal(move))
	{
		return move;
	}
	return Move::MOVE_NONE;
}
