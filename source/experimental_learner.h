#ifndef _LEARNER_H_
#define _LEARNER_H_

#include <sstream>

#include "position.h"

namespace Learner
{
  struct Record {
    PackedSfen packed;
    int16_t value;
  };

  constexpr char* OPTION_GENERATOR_NUM_GAMES = "GeneratorNumGames";
  constexpr char* OPTION_GENERATOR_MIN_SEARCH_DEPTH = "GeneratorMinSearchDepth";
  constexpr char* OPTION_GENERATOR_MAX_SEARCH_DEPTH = "GeneratorMaxSearchDepth";
  constexpr char* OPTION_GENERATOR_KIFU_TAG = "GeneratorKifuTag";
  constexpr char* OPTION_GENERATOR_BOOK_FILE_NAME = "GeneratorStartposFileName";
  constexpr char* OPTION_GENERATOR_MIN_BOOK_MOVE = "GeneratorMinBookMove";
  constexpr char* OPTION_GENERATOR_MAX_BOOK_MOVE = "GeneratorMaxBookMove";
  constexpr char* OPTION_LEARNER_NUM_POSITIONS = "LearnerNumPositions";

  void Learn(std::istringstream& iss);
  void MeasureError();
  void BenchmarkKifuReader();
}

#endif
