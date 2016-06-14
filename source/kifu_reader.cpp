#include "kifu_reader.h"

#include <sstream>
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX

#include "misc.h"

namespace {
  constexpr int kBatchSize = 10000000;
  constexpr int kMaxLoop = 1;
  constexpr Value kCloseOutValueThreshold = Value(2000);
}

Learner::KifuReader::KifuReader(const std::string& folder_name)
  : folder_name_(folder_name) {
}

Learner::KifuReader::~KifuReader() {
  Close();
}

bool Learner::KifuReader::Read(Position& pos, Value& value) {
  std::string sfen;
  {
    std::lock_guard<std::mutex> lock_guard(mutex_);

    if (!EnsureOpen()) {
      return false;
    }

    if (record_index_ >= static_cast<int>(records_.size())) {
      records_.clear();
      while (records_.size() < kBatchSize) {
        std::string line;
        if (!std::getline(file_stream_, line)) {
          // ファイルの終端に辿り着いた
          if (++file_index_ >= static_cast<int>(file_paths_.size())) {
            // ファイルリストの終端にたどり着いた
            if (++loop_ >= kMaxLoop) {
              // 読み込み回数の上限に達したため
              // これ以上の読み込みは行わない
              // 読み込み回数の上限に達して以降2回目以降もここに来る
              break;
            }

            // ファイルインデクスをリセットする
            file_index_ = 0;
          }

          // 次のファイルを開く
          file_stream_.close();
          file_stream_.open(file_paths_[file_index_]);

          if (!file_stream_.is_open()) {
            // ファイルを開くのに失敗したら
            // 読み込みを終了する
            sync_cout << "into string Failed to open a kifu file: "
              << file_paths_[0] << sync_endl;
            return false;
          }

          if (!std::getline(file_stream_, line)) {
            // 空のファイルの場合はここに来る
            // とりあえずcontinueしてつぎのファイルを試す
            continue;
          }
        }

        size_t offset = line.find(',');
        int value = std::atoi(line.substr(offset + 1).c_str());
        if (abs(value) > kCloseOutValueThreshold) {
          // 評価値の絶対値が大きすぎる場合は使用しない
          continue;
        }

        records_.push_back({
          line.substr(0, offset),
          static_cast<Value>(value) });
      }

      std::random_shuffle(records_.begin(), records_.end());
      record_index_ = 0;
    }

    if (records_.empty()) {
      return false;
    }

    sfen = records_[record_index_].sfen;
    value = records_[record_index_].value;
    ++record_index_;
  }

  // パフォーマンス向上のためPosition::set()はロックの外側で呼び出す
  pos.set(sfen);
  return true;
}

bool Learner::KifuReader::Close() {
  file_stream_.close();
  return true;
}

bool Learner::KifuReader::EnsureOpen() {
  if (!file_paths_.empty()) {
    return true;
  }

  // http://qiita.com/tkymx/items/f9190c16be84d4a48f8a
  HANDLE find = nullptr;
  WIN32_FIND_DATAA find_data = { 0 };

  std::string search_name = folder_name_ + "/*.*";
  find = FindFirstFileA(search_name.c_str(), &find_data);

  if (find == INVALID_HANDLE_VALUE) {
    sync_cout << "info string Failed to find kifu files." << sync_endl;
    return false;
  }

  do {
    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      continue;
    }

    std::string file_path = folder_name_ + "/" + find_data.cFileName;
    file_paths_.push_back(file_path);
  } while (FindNextFileA(find, &find_data));

  FindClose(find);
  find = nullptr;

  if (file_paths_.empty()) {
    return false;
  }

  file_stream_.open(file_paths_[0]);

  if (!file_stream_.is_open()) {
    sync_cout << "into string Failed to open a kifu file: " << file_paths_[0]
      << sync_endl;
    return false;
  }

  return true;
}
