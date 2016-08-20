#ifndef _KIFU_READER_H_
#define _KIFU_READER_H_

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "experimental_learner.h"
#include "position.h"
#include "shogi.h"

namespace Learner {

  class KifuReader {
  public:
    KifuReader(const std::string& file_path, bool shuffle);
    virtual ~KifuReader();
    // 棋譜ファイルからデータを1局面読み込む
    // 棋譜の読み込みは対称のフォルダ内の複数のファイルから行う
    // ファイルの順番をシャッフルし、
    // 先頭のファイルから順にkBatchSize局ずつ面読み込み、
    // 再度シャッフル、その後num_recordsずつ返す
    bool Read(Record& record);
    bool Close();

  private:

    const std::string file_path_;
    FILE* file_ = nullptr;
    int record_index_ = 0;
    std::vector<Record> records_;
    std::vector<int> permutation_;
    bool shuffle_;

    bool EnsureOpen();
  };

}

#endif
