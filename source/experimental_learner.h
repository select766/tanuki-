#ifndef _LEARNER_H_
#define _LEARNER_H_

#include <sstream>

#include "position.h"
#include "shogi.h"

namespace Learner {
enum GameResult {
    GameResultWin = 1,
    GameResultLose = -1,
    GameResultDraw = 0,
};

constexpr char* kOptionValueValueToWinningRateCoefficient = "ValueToWinningRateCoefficient";

double ToWinningRate(double value, double value_to_winning_rate_coefficient);
double ToValue(double winning_rate, double value_to_winning_rate_coefficient);

#ifdef USE_EXPERIMENTAL_LEARNER
void InitializeLearner(USI::OptionsMap& o);
void Learn(std::istringstream& iss);
void CalculateTestDataEntropy(std::istringstream& iss);
void BreedEval(std::istringstream& iss);
void GenerateInitialEval(std::istringstream& iss);
#endif
}

#endif
