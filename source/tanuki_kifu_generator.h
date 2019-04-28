#ifndef _TANUKI_KIFU_GENERATOR_H_
#define _TANUKI_KIFU_GENERATOR_H_

#include "extra/config.h"

#ifdef EVAL_LEARN

#include "shogi.h"

namespace Tanuki {
	void InitializeGenerator(USI::OptionsMap& o);
	void GenerateKifu();
	void ConvertSfenToLearningData();
}

#endif

#endif
