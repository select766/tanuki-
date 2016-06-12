#ifndef _KIFU_WRITER_H_
#define _KIFU_WRITER_H_

#include <mutex>

#include "position.h"
#include "shogi.h"

namespace Learner {

  class KifuWriter
  {
  public:
    KifuWriter(const std::string& output_file_path_format);
    virtual ~KifuWriter();
    bool Write(Position& pos, Value value);
    bool Close();

  private:
    const std::string output_file_path_format_;
    FILE* file_ = nullptr;
    int file_index_ = 0;
    int64_t position_index_ = 0;
    std::mutex mutex_;
    std::string file_path_;

    bool EnsureOpen();
  };

}

#endif
