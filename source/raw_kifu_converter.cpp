#include "raw_kifu_converter.h"

#include <atomic>
#include <direct.h>
#include <fstream>
#include <omp.h>
#include <sstream>

#include "experimental_learner.h"
#include "kifu_writer.h"
#include "thread.h"

namespace Learner {
  std::pair<Value, std::vector<Move> > search(Position& pos, Value alpha, Value beta, int depth);
}

void Learner::ConvertRawKifu() {
#ifdef USE_FALSE_PROBE_IN_TT
  ASSERT_LV3(false);
#endif
  Eval::load_eval();
  omp_set_num_threads((int)Options["Threads"]);
  std::string kifu_dir = (std::string)Options["KifuDir"];
  std::string kifu_tag = (std::string)Options[Learner::OPTION_CONVERTER_KIFU_TAG];
  int64_t num_positions;
  std::istringstream((std::string)Options[Learner::OPTION_CONVERTER_NUM_POSITIONS]) >> num_positions;
  std::string raw_kifu_file_path_format =
    (std::string)Options[Learner::OPTION_CONVERTER_RAW_KIFU_FILE_PATH_FORMAT];
  int min_search_depth = (int)Options[Learner::OPTION_CONVERTER_MIN_SEARCH_DEPTH];
  int max_search_depth = (int)Options[Learner::OPTION_CONVERTER_MAX_SEARCH_DEPTH];
  std::uniform_int_distribution<> search_depth_distribution(min_search_depth, max_search_depth);
  std::atomic_int64_t position_index = 0;
  mkdir(kifu_dir.c_str());
  auto start = std::chrono::system_clock::now();
#pragma omp parallel
  {
    int thread_index = ::omp_get_thread_num();
    char raw_kifu_file_path[1024];
    sprintf(raw_kifu_file_path, raw_kifu_file_path_format.c_str(), thread_index);
    std::ifstream ifs(raw_kifu_file_path);
    ASSERT_LV3(ifs);
    char output_kifu_file_path[1024];
    sprintf(output_kifu_file_path, "%s/kifu.%s.%d-%d.%lld.%02d.bin", kifu_dir.c_str(),
      kifu_tag.c_str(), min_search_depth, max_search_depth, num_positions, thread_index);
    KifuWriter writer(output_kifu_file_path);
    Thread& thread = *Threads[thread_index];
    Position& pos = thread.rootPos;
    std::mt19937_64 mt(start.time_since_epoch().count() + thread_index);

    std::string sfen;
    while (position_index < num_positions && std::getline(ifs, sfen)) {
      pos.set(sfen);
      //sync_cout << pos << sync_endl;
      pos.set_this_thread(&thread);
      if (pos.is_mated()) {
        continue;
      }
      if (!pos.pos_is_ok()) {
        continue;
      }

      int search_depth = search_depth_distribution(mt);
      auto value_and_pv = Learner::search(pos, -VALUE_INFINITE, VALUE_INFINITE, search_depth);
      Value value = value_and_pv.first;
      const auto& pv = value_and_pv.second;
      if (pv.empty()) {
        break;
      }

      Learner::Record record;
      pos.sfen_pack(record.packed);
      record.value = value;
      writer.Write(record);
      int local_position_index = position_index++;
      ShowProgress(start, local_position_index, num_positions, 1000000);
    }
  }
}
