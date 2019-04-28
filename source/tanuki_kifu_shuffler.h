#ifndef _TANUKI_KIFU_SHUFFLER_H_
#define _TANUKI_KIFU_SHUFFLER_H_

#include "extra/config.h"

#ifdef EVAL_LEARN

#include "shogi.h"

namespace Tanuki {
	void InitializeShuffler(USI::OptionsMap& o);
	void ShuffleKifu();
};

#endif

#endif
