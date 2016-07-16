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
    KifuReader(const std::string& folder_name, bool shuffle);
    virtual ~KifuReader();
    // 棋譜ファイルからデータを読み込む
    // 棋譜の読み込みは対称のフォルダ内の複数のファイルから行う
    // ファイルの順番をシャッフルし、
    // 先頭のファイルから順にkBatchSize局ずつ面読み込み、
    // 再度シャッフル、その後num_recordsずつ返す
    bool Read(int num_records, std::vector<Record>& records);
    bool Close();

  private:
    // 棋譜ファイルからデータを1局面読み込む
    bool Read(Record& record);

    const std::string folder_name_;
    std::vector<std::string> file_paths_;
    FILE* file_;
    int loop_ = 0;
    int file_index_ = 0;
    int record_index_ = 0;
    std::vector<Record> records_;
    bool shuffle_;

    bool EnsureOpen();
  };

}

#endif
