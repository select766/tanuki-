#include "kifu_writer.h"

#include "misc.h"

namespace {
  constexpr int64_t kMaxPositionsPerFile = 100000000;
  constexpr int64_t kFlushPerPositions = 1000000;
  constexpr int kBufferSize = 1 << 24; // 16MB
}

Learner::KifuWriter::KifuWriter(const std::string& output_file_path_format) :
  output_file_path_format_(output_file_path_format) {
}

Learner::KifuWriter::~KifuWriter() {
  Close();
}

bool Learner::KifuWriter::Write(Position& pos, Value value) {
  std::lock_guard<std::mutex> lock_guard(mutex_);

  if (!EnsureOpen()) {
    return false;
  }

  std::fprintf(file_, "%s,%d\n", pos.sfen().c_str(), static_cast<int>(value));
  ++position_index_;

  if (position_index_ % kFlushPerPositions == 0) {
    if (fflush(file_)) {
      sync_cout << "info string Failed to flush the output kifu file: kifu_file_path_=" << file_path_ << sync_endl;
      return false;
    }
  }
  return true;
}

bool Learner::KifuWriter::Close() {
  if (!file_) {
    return true;
  }

  bool result = true;
  if (std::fclose(file_) != 0) {
    sync_cout << "info string Failed to close the output kifu file: kifu_file_path_=" << file_path_ << sync_endl;
    result = false;
  }
  file_ = nullptr;
  return result;
}

bool Learner::KifuWriter::EnsureOpen() {
  if (!file_ || position_index_ >= kMaxPositionsPerFile) {
    if (!Close()) {
      return false;
    }
    position_index_ = 0;

    char file_path_buffer[1024];
    std::sprintf(file_path_buffer, output_file_path_format_.c_str(),
      file_index_);
    file_path_ = file_path_buffer;
    file_ = std::fopen(file_path_buffer, "wt");
    if (!file_) {
      sync_cout << "info string Failed to open the output kifu file: kifu_file_path_=" << file_path_ << sync_endl;
      return false;
    }

    if (std::setvbuf(file_, nullptr, _IOFBF, kBufferSize)) {
      sync_cout << "info string Failed to set the output buffer: kifu_file_path_=" << file_path_ << sync_endl;
      return false;
    }
    ++file_index_;
  }

  return true;
}
