#ifndef _PROGRESS_REPORT_H_
#define _PROGRESS_REPORT_H_

#include <cstdint>

class ProgressReport {
public:
  ProgressReport(int64_t total_data, int64_t show_at_most_sec);
  void Show(int64_t current_data);

private:
  const int64_t total_data_;
  const int64_t show_at_most_sec_;
  int64_t previous_data_ = 0;
  time_t start_time_;
  time_t previous_time_;
  double exponential_moving_averaged_data = 0.0;
  double exponential_moving_averaged_time = 0.0;
};

#endif
