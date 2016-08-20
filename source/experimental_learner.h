#ifndef _LEARNER_H_
#define _LEARNER_H_

#include <sstream>

#include "shogi.h"

namespace Learner
{
  struct Record {
    uint8_t packed[32];
    int16_t value;
  };

  constexpr Depth kSearchDepth = Depth(3);
  constexpr int kNumGamesToGenerateKifu = 2000'0000;

  void Learn(std::istringstream& iss);
  void MeasureError();
  void BenchmarkKifuReader();
  std::string GenerateKifuFilePath(int thread_index);
}

#endif
