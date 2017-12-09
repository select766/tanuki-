#ifndef _LEARNER_H_
#define _LEARNER_H_

#include <sstream>

#include "position.h"
#include "shogi.h"

namespace Learner {
struct Record {
    PackedSfen packed;
    int16_t value;
    int16_t weighted_value;
    int8_t win_color;
    int8_t last_position;
};
static_assert(sizeof(Record) == 38, "Size of Record is not 38");

constexpr char* kOptionValueValueToWinningRateCoefficient = "ValueToWinningRateCoefficient";

double ToWinningRate(Value value, double value_to_winning_rate_coefficient);
Value ToValue(double winning_rate, double value_to_winning_rate_coefficient);

#ifdef USE_EXPERIMENTAL_LEARNER
void InitializeLearner(USI::OptionsMap& o);
void Learn(std::istringstream& iss);
void CalculateTestDataEntropy(std::istringstream& iss);
void BreedEval(std::istringstream& iss);
void GenerateInitialEval(std::istringstream& iss);
#endif
}

#endif
