#ifndef _TANUKI_KIFU_SHUFFLER_H_
#define _TANUKI_KIFU_SHUFFLER_H_

#include "config.h"

#ifdef EVAL_LEARN

#include "usi.h"

namespace Tanuki {
	void InitializeShuffler(USI::OptionsMap& o);
	void ShuffleKifu(Position& position);
};

#endif

#endif
