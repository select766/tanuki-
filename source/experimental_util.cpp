#include "experimental_util.h"

#include <algorithm>
#include <cstdio>
#include <limits>

void Utility::ShowProgress(const time_t& start_time, int64_t current_data, int64_t total_data,
  int64_t show_per) {
  if (!current_data || current_data % show_per) {
    return;
  }
  time_t current_time = 0;
  std::time(&current_time);
  if (start_time == current_time) {
    return;
  }

  time_t elapsed_time = current_time - start_time;
  int hour = elapsed_time / 3600;
  int minute = elapsed_time / 60 % 60;
  int second = elapsed_time % 60;
  double data_per_time = current_data / static_cast<double>(current_time - start_time);
  time_t expected_time = std::min<time_t>(current_time + (total_data - current_data) / data_per_time, std::numeric_limits<int>::max());
  struct tm current_tm = *std::localtime(&current_time);
  struct tm expected_tm = *std::localtime(&expected_time);
  std::printf(
    "%lld / %lld\n"
    "        elapsed time = %02d:%02d:%02d\n"
    "   current date time = %04d-%02d-%02d %02d:%02d:%02d\n"
    "    finish date time = %04d-%02d-%02d %02d:%02d:%02d\n",
    current_data, total_data,
    hour, minute, second,
    current_tm.tm_year + 1900, current_tm.tm_mon + 1, current_tm.tm_mday, current_tm.tm_hour, current_tm.tm_min, current_tm.tm_sec,
    expected_tm.tm_year + 1900, expected_tm.tm_mon + 1, expected_tm.tm_mday, expected_tm.tm_hour, expected_tm.tm_min, expected_tm.tm_sec);
  std::fflush(stdout);
}
