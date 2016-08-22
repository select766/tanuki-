#include "kifu_reader.h"

#include <sstream>
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX

#include "misc.h"

namespace {
  constexpr int kBatchSize = 1000'0000;
  constexpr int kBufferSize = 1 << 24; // 16MB
}

Learner::KifuReader::KifuReader(const std::string& file_path, bool shuffle)
  : file_path_(file_path), shuffle_(shuffle) {
  for (int i = 0; i < kBatchSize; ++i) {
    permutation_.push_back(i);
  }
  if (shuffle) {
    std::shuffle(permutation_.begin(), permutation_.end(), std::mt19937_64(std::random_device()()));
  }
}

Learner::KifuReader::~KifuReader() {
  Close();
}

bool Learner::KifuReader::Read(Record& record) {
  if (!EnsureOpen()) {
    return false;
  }

  if (record_index_ >= static_cast<int>(records_.size())) {
    records_.resize(kBatchSize);
    int num_records = 0;
    do {
      int read_count = std::fread(&records_[num_records], sizeof(record), kBatchSize - num_records, file_);
      num_records += read_count;
      if (read_count == 0) {
        std::fseek(file_, 0, SEEK_SET);
      }
    } while (num_records < kBatchSize);
    record_index_ = 0;
  }

  record = records_[permutation_[record_index_++]];

  return true;
}

bool Learner::KifuReader::Close() {
  if (fclose(file_)) {
    return false;
  }
  file_ = nullptr;
  return true;
}

bool Learner::KifuReader::EnsureOpen() {
  if (file_) {
    return true;
  }

  file_ = std::fopen(file_path_.c_str(), "rb");

  if (!file_) {
    sync_cout << "into string Failed to open a kifu file: " << file_path_[0]
      << sync_endl;
    return false;
  }

  if (std::setvbuf(file_, nullptr, _IOFBF, kBufferSize)) {
    sync_cout << "into string Failed to set a file buffer: "
      << file_path_[0] << sync_endl;
  }

  return true;
}
