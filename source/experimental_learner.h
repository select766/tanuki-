#ifndef _LEARNER_H_
#define _LEARNER_H_

#include <sstream>

#include "position.h"
#include "shogi.h"

namespace Learner
{
  struct Record {
    PackedSfen packed;
    int16_t value;
  };
  static_assert(sizeof(Record) == 34, "Size of Record is not 34");

  void InitializeLearner(USI::OptionsMap& o);
  void Learn(std::istringstream& iss);
}

#endif
