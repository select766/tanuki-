#include "kifu_generator.h"

#include <atomic>
#include <ctime>
#include <direct.h>
#include <fstream>
#include <memory>
#include <omp.h>
#include <random>
#include <sstream>

#include "kifu_writer.h"
#include "search.h"
#include "thread.h"

using Search::RootMove;
using USI::Option;
using USI::OptionsMap;

namespace Learner
{
  std::pair<Value, std::vector<Move> > search(Position& pos, Value alpha, Value beta, int depth);
  std::pair<Value, std::vector<Move> > qsearch(Position& pos, Value alpha, Value beta);
}

namespace
{
  constexpr int kMaxGamePlay = 256;
  constexpr int kMaxSwapTrials = 10;
  constexpr int kMaxTrialsToSelectSquares = 100;
  constexpr int kShowProgressPerPositions = 1000'0000;

  constexpr char* kOptionGeneratorNumPositions = "GeneratorNumPositions";
  constexpr char* kOptionGeneratorMinSearchDepth = "GeneratorMinSearchDepth";
  constexpr char* kOptionGeneratorMaxSearchDepth = "GeneratorMaxSearchDepth";
  constexpr char* kOptionGeneratorKifuTag = "GeneratorKifuTag";
  constexpr char* kOptionGeneratorStartposFileName = "GeneratorStartposFileName";
  constexpr char* kOptionGeneratorMinBookMove = "GeneratorMinBookMove";
  constexpr char* kOptionGeneratorMaxBookMove = "GeneratorMaxBookMove";
  constexpr char* kOptionGeneratorValueThreshold = "GeneratorValueThreshold";
  constexpr char* kOptionGeneratorDoRandomKingMoveProbability = "GeneratorDoRandomKingMoveProbability";
  constexpr char* kOptionGeneratorSwapTwoPiecesProbability = "GeneratorSwapTwoPiecesProbability";
  constexpr char* kOptionGeneratorDoRandomMoveProbability = "GeneratorDoRandomMoveProbability";
  constexpr char* kOptionGeneratorDoRandomMoveAfterBook = "GeneratorDoRandomMoveAfterBook";
  constexpr char* kOptionGeneratorWriteAnotherPosition = "GeneratorWriteAnotherPosition";

  std::vector<std::string> book;
  std::uniform_real_distribution<> probability_distribution;

  bool ReadBook()
  {
    // 定跡ファイル(というか単なる棋譜ファイル)の読み込み
    std::string book_file_name = Options[kOptionGeneratorStartposFileName];
    std::ifstream fs_book;
    fs_book.open(book_file_name);

    if (!fs_book.is_open())
    {
      sync_cout << "Error! : can't read " << book_file_name << sync_endl;
      return false;
    }

    sync_cout << "read book.sfen " << sync_endl;
    std::string line;
    while (!fs_book.eof())
    {
      std::getline(fs_book, line);
      if (!line.empty())
        book.push_back(line);
      if ((book.size() % 1000) == 0)
        std::cout << ".";
    }
    std::cout << std::endl;
    return true;
  }

  template<typename T>
  T ParseOptionOrDie(const char* name) {
    std::string value_string = (std::string)Options[name];
    std::istringstream iss(value_string);
    T value;
    if (!(iss >> value)) {
      sync_cout << "Failed to parse an option. Exitting...: name=" << name << " value=" << value << sync_endl;
      std::exit(1);
    }
    return value;
  }

  // 王が移動する指し手からランダムに1手選んで指す
  bool DoRandomKingMove(Position& pos, std::mt19937_64& mt, StateInfo* state) {
    MoveList<LEGAL> list(pos);
    ExtMove* it2 = (ExtMove*)list.begin();
    for (ExtMove* it = (ExtMove*)list.begin(); it != list.end(); ++it)
      if (type_of(pos.moved_piece_after(it->move)) == KING)
        *it2++ = *it;

    auto size = it2 - list.begin();
    if (size == 0) {
      return false;
    }

    // ランダムにひとつ選ぶ
    pos.do_move(list.at(std::uniform_int_distribution<>(0, static_cast<int>(size) - 1)(mt)),
      state[pos.game_ply()]);
    // 差分計算のためevaluate()を呼び出す
    Eval::evaluate(pos);
    return true;
  }

  // 2駒をランダムに入れ替える
  // https://github.com/yaneurao/YaneuraOu/blob/master/source/learn/learner.cpp
  bool SwapTwoPieces(Position& pos, std::mt19937_64& mt) {
    for (int retry = 0; retry < 10; ++retry) {
      // 手番側の駒を2駒入れ替える。

      // 与えられたBitboardからランダムに1駒を選び、そのSquareを返す。
      auto get_one = [&mt](Bitboard pieces) {
        // 駒の数
        int num = pieces.pop_count();

        // 何番目かの駒
        int n = std::uniform_int_distribution<>(1, num)(mt);
        Square sq = SQ_NB;
        for (int i = 0; i < n; ++i) {
          sq = pieces.pop();
        }
        return sq;
      };

      // この升の2駒を入れ替える。
      auto pieces = pos.pieces(pos.side_to_move());

      auto sq1 = get_one(pieces);
      // sq1を除くbitboard
      auto sq2 = get_one(pieces ^ sq1);

      // sq2は王しかいない場合、SQ_NBになるから、これを調べておく
      // この指し手に成功したら、それはdo_moveの代わりであるから今回、do_move()は行わない。

      if (sq2 != SQ_NB && pos.do_move_by_swapping_pieces(sq1, sq2)) {
        // 差分計算のためevaluate()を呼び出す
        Eval::evaluate(pos);
        // 検証用のassert
        if (!is_ok(pos)) {
          sync_cout << pos << sq1 << sq2 << sync_endl;
        }

        return true;
      }
    }

    return false;
  }

  // 合法手の中からランダムに1手指す
  //https://github.com/yaneurao/YaneuraOu/blob/master/source/learn/learner.cpp
  bool DoRandomMove(Position& pos, std::mt19937_64& mt, StateInfo* state, Move& move) {
    // 合法手のなかからランダムに1手選ぶフェーズ
    MoveList<LEGAL> list(pos);
    move = list.at(std::uniform_int_distribution<>(0, static_cast<int>(list.size()) - 1)(mt));
    pos.do_move(move, state[pos.game_ply()]);
    // 差分計算のためevaluate()を呼び出す
    Eval::evaluate(pos);
    return true;
  }

  enum SearchAndWriteResult {
    kSearchAndWriteUnknown,
    kSearchAndWriteSucceeded,
    kSearchAndWriteInvalidState,
    kSearchAndWriteValueThresholdExceeded,
    kSearchAndWriteNumPositionReached,
  };

  // 探索を行い、問題がなければ結果を書き出す
  // return 成功した場合はtrue、そうでない場合はfalse。falseの場合は対局を中止したほうが良い
  SearchAndWriteResult SearchAndWrite(
    const std::uniform_int_distribution<>& search_depth_distribution, int value_threshold,
    time_t start_time, int64_t num_positions, std::mt19937_64& mt19937_64, Position& pos,
    Learner::KifuWriter& kifu_writer, std::atomic_int64_t& global_position_index,
    Move& pv_move) {
    int search_depth = search_depth_distribution(mt19937_64);
    auto valueAndPv = Learner::search(pos, -VALUE_INFINITE, VALUE_INFINITE, search_depth);

    // Aperyでは後手番でもスコアの値を反転させずに学習に用いている
    Value value = valueAndPv.first;
    const std::vector<Move>& pv = valueAndPv.second;
    if (pv.empty()) {
      return kSearchAndWriteInvalidState;
    }

    // 評価値の絶対値の上限を超えている場合は書き出さないようにする
    if (std::abs(value) > value_threshold) {
      return kSearchAndWriteValueThresholdExceeded;
    }

    // 局面が不正な場合があるので再度チェックする
    if (!pos.pos_is_ok()) {
      return kSearchAndWriteInvalidState;
    }

    // 必要局面数生成したら終了する
    int64_t position_index = global_position_index++;
    if (position_index >= num_positions) {
        return kSearchAndWriteNumPositionReached;
    }

    Learner::Record record = { 0 };
    pos.sfen_pack(record.packed);
    record.value = value;

    if (!kifu_writer.Write(record)) {
      return kSearchAndWriteInvalidState;
    }

    Learner::ShowProgress(start_time, position_index, num_positions, kShowProgressPerPositions);
    pv_move = pv[0];
    return kSearchAndWriteSucceeded;
  }
}

void Learner::InitializeGenerator(USI::OptionsMap& o) {
  o[kOptionGeneratorNumPositions] << Option("10000000000");
  o[kOptionGeneratorMinSearchDepth] << Option(3, 1, MAX_PLY);
  o[kOptionGeneratorMaxSearchDepth] << Option(4, 1, MAX_PLY);
  o[kOptionGeneratorKifuTag] << Option("default_tag");
  o[kOptionGeneratorStartposFileName] << Option("startpos.sfen");
  o[kOptionGeneratorMinBookMove] << Option(0, 1, MAX_PLY);
  o[kOptionGeneratorMaxBookMove] << Option(32, 1, MAX_PLY);
  o[kOptionGeneratorValueThreshold] << Option(VALUE_MATE, 0, VALUE_MATE);
  o[kOptionGeneratorDoRandomKingMoveProbability] << Option("0.1");
  o[kOptionGeneratorSwapTwoPiecesProbability] << Option("0.1");
  o[kOptionGeneratorDoRandomMoveProbability] << Option("0.1");
  o[kOptionGeneratorDoRandomMoveAfterBook] << Option(true);
  o[kOptionGeneratorWriteAnotherPosition] << Option(true);
}

void Learner::GenerateKifu()
{
#ifdef USE_FALSE_PROBE_IN_TT
  sync_cout << "Please undefine USE_FALSE_PROBE_IN_TT." << sync_endl;
  ASSERT_LV3(false);
#endif

  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  // 定跡の読み込み
  if (!ReadBook()) {
    sync_cout << "Failed to read the book." << sync_endl;
    return;
  }

  Eval::load_eval();

  omp_set_num_threads((int)Options["Threads"]);

  Search::LimitsType limits;
  limits.max_game_ply = kMaxGamePlay;
  limits.depth = MAX_PLY;
  limits.silent = true;
  Search::Limits = limits;

  std::string kifu_directory = (std::string)Options["KifuDir"];
  _mkdir(kifu_directory.c_str());

  int min_search_depth = Options[kOptionGeneratorMaxSearchDepth];
  int max_search_depth = Options[kOptionGeneratorMaxSearchDepth];
  std::uniform_int_distribution<> search_depth_distribution(min_search_depth, max_search_depth);
  int min_book_move = Options[kOptionGeneratorMinBookMove];
  int max_book_move = Options[kOptionGeneratorMaxBookMove];
  std::uniform_int_distribution<> num_book_move_distribution(min_book_move, max_book_move);
  int64_t num_positions = ParseOptionOrDie<int64_t>(kOptionGeneratorNumPositions);
  double do_random_king_move_probability =
    ParseOptionOrDie<double>(kOptionGeneratorDoRandomKingMoveProbability);
  double swap_two_pieces_probability =
    ParseOptionOrDie<double>(kOptionGeneratorSwapTwoPiecesProbability);
  double do_random_move_probability =
    ParseOptionOrDie<double>(kOptionGeneratorDoRandomMoveProbability);
  bool do_random_move_after_book = (bool)Options[kOptionGeneratorDoRandomMoveAfterBook];
  int value_threshold = Options[kOptionGeneratorValueThreshold];
  std::string output_file_name_tag = Options[kOptionGeneratorKifuTag];
  bool write_another_position = (bool)Options[kOptionGeneratorWriteAnotherPosition];

  time_t start_time;
  std::time(&start_time);
  ASSERT_LV3(book.size());
  std::uniform_int<> opening_index(0, static_cast<int>(book.size() - 1));
  // スレッド間で共有する
  std::atomic_int64_t global_position_index = 0;
#pragma omp parallel
  {
    int thread_index = ::omp_get_thread_num();
    char output_file_path[1024];
    std::sprintf(output_file_path, "%s/kifu.%s.%d-%d.%I64d.%03d.bin", kifu_directory.c_str(),
      output_file_name_tag.c_str(), min_search_depth, max_search_depth, num_positions,
      thread_index);
    // 各スレッドに持たせる
    std::unique_ptr<Learner::KifuWriter> kifu_writer =
      std::make_unique<Learner::KifuWriter>(output_file_path);
    std::mt19937_64 mt19937_64(start_time + thread_index);

    while (global_position_index < num_positions) {
      Thread& thread = *Threads[thread_index];
      Position& pos = thread.rootPos;
      pos.set_hirate();
      StateInfo state_infos[512] = { 0 };
      StateInfo* state = state_infos + 8;

      const std::string& opening = book[opening_index(mt19937_64)];
      std::istringstream is(opening);
      std::string token;
      int num_book_move = num_book_move_distribution(mt19937_64);
      while (global_position_index < num_positions && pos.game_ply() < num_book_move)
      {
        if (!(is >> token)) {
          break;
        }
        if (token == "startpos" || token == "moves")
          continue;

        Move m = move_from_usi(pos, token);
        if (!is_ok(m) || !pos.legal(m))
        {
          //  sync_cout << "Error book.sfen , line = " << book_number << " , moves = " << token << endl << rootPos << sync_endl;
          // →　エラー扱いはしない。
          break;
        }

        pos.do_move(m, state[pos.game_ply()]);
        // 差分計算のためevaluate()を呼び出す
        Eval::evaluate(pos);
      }

      if (do_random_move_after_book) {
        // 開始局面からランダムに手を指して、局面をバラけさせる
        Move move = Move::MOVE_NONE;
        DoRandomMove(pos, mt19937_64, state, move);
        // 差分計算のためevaluate()を呼び出す
        Eval::evaluate(pos);
      }

      while (pos.game_ply() < kMaxGamePlay && !pos.is_mated()) {
        pos.set_this_thread(&thread);

        Move pv_move = Move::MOVE_NONE;
        auto result = SearchAndWrite(search_depth_distribution, value_threshold, start_time,
          num_positions, mt19937_64, pos, *kifu_writer, global_position_index, pv_move);
        if (result != kSearchAndWriteSucceeded) {
          break;
        }

        // 指定した確率に従って特別な指し手を指す
        double r = probability_distribution(mt19937_64);
        bool special_move_is_done = false;
        if (r < do_random_king_move_probability) {
          if (!pos.in_check()) {
            special_move_is_done = DoRandomKingMove(pos, mt19937_64, state);
          }
        }
        else if (r < do_random_king_move_probability + swap_two_pieces_probability) {
          if (!pos.in_check() && pos.pieces(pos.side_to_move()).pop_count() >= 6) {
            special_move_is_done = SwapTwoPieces(pos, mt19937_64);
          }
        }
        else if (r < do_random_king_move_probability + swap_two_pieces_probability +
          do_random_move_probability) {
          if (!pos.in_check()) {
            Move dummy_move = Move::MOVE_NONE;
            special_move_is_done = DoRandomMove(pos, mt19937_64, state, dummy_move);
          }
        }

        // 特別な指し手が指されなかった場合は探索による指し手を指す
        if (!special_move_is_done) {
          if (write_another_position) {
            // ランダムに選んだ指し手を学習データに加える
            Move move = Move::MOVE_NONE;
            DoRandomMove(pos, mt19937_64, state, move);
            // 差分計算のためevaluate()を呼び出す
            Eval::evaluate(pos);

            if (!pos.is_mated()) {
              Move dummy_move = Move::MOVE_NONE;
              auto result = SearchAndWrite(search_depth_distribution, value_threshold, start_time,
                num_positions, mt19937_64, pos, *kifu_writer, global_position_index, dummy_move);
              if (result != kSearchAndWriteSucceeded &&
                result != kSearchAndWriteValueThresholdExceeded) {
                break;
              }
            }

            pos.undo_move(move);
          }

          pos.do_move(pv_move, state[pos.game_ply()]);
          // 差分計算のためevaluate()を呼び出す
          Eval::evaluate(pos);
        }
      }
    }

    // 必要局面数生成したら全スレッドの探索を停止する
    // こうしないと相入玉等合法手の多い局面で止まるまでに時間がかかる
    Search::Signals.stop = true;
  }
}
