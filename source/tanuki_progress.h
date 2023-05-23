#ifndef _TANUKI_PROGRESS_H
#define _TANUKI_PROGRESS_H

#include <map>
#include <string>

#include "evaluate.h"
#include "usi.h"

class Position;

namespace Tanuki {
	class Progress {
	public:
		static bool Initialize(USI::OptionsMap& o);
		bool Load();
#ifdef EVAL_LEARN
		bool Save();
		bool Learn();
#endif // EVAL_LEARN
		double Estimate(const Position& pos);

	private:
		double weights_[SQ_NB][Eval::BonaPiece::fe_end] = {};
	};
}

#endif
