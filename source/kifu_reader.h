#ifndef _KIFU_READER_H_
#define _KIFU_READER_H_

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "position.h"
#include "shogi.h"

namespace Learner {

  struct Record {
    std::string sfen;
    Value value;
  };

  class KifuReader
  {
  public:
    KifuReader(const std::string& folder_name);
    virtual ~KifuReader();
    // 棋譜ファイルからデータを読み込む
    // 棋譜の読み込みは対称のフォルダ内の複数のファイルから行う
    // 先頭のファイルから順にkBatchSize局ずつ面読み込み、
    // シャッフルして使用する
    // 最大でkMaxLoop回まで、複数のファイル全体を読み込む
    bool Read(Position& pos, Value& value);
    bool Close();

  private:
    const std::string folder_name_;
    std::vector<std::string> file_paths_;
    std::ifstream file_stream_;
    int loop_ = 0;
    int file_index_ = 0;
    int record_index_ = 0;
    std::vector<Record> records_;
    std::mutex mutex_;

    bool EnsureOpen();
  };

}

#endif
