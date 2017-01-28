#ifndef _EXPERIMENTAL_UTIL_H
#define _EXPERIMENTAL_UTIL_H

#include <cstdint>
#include <ctime>

namespace Utility {
  void ShowProgress(const time_t& start_time, int64_t current_data, int64_t total_data,
    int64_t show_per);
}

#endif
