#ifndef _LEARNER_H_
#define _LEARNER_H_

#include <sstream>

namespace Learner
{
  struct Record {
    uint8_t packed[32];
    int16_t value;
  };

  void Learn(std::istringstream& iss);
  void MeasureError();
  void BenchmarkKifuReader();
}

#endif
