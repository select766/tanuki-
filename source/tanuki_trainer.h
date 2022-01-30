#ifndef _TANUKI_TRAINER_H_
#define _TANUKI_TRAINER_H_

#ifdef EVAL_LEARN

#include "usi.h"

namespace Tanuki {
	void InitializeTrainer(USI::OptionsMap& o);
	void Train(std::istringstream& iss);
}

#endif

#endif
