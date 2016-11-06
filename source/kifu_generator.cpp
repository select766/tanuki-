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

  constexpr char* OPTION_GENERATOR_NUM_POSITIONS = "GeneratorNumPositions";
  constexpr char* OPTION_GENERATOR_MIN_SEARCH_DEPTH = "GeneratorMinSearchDepth";
  constexpr char* OPTION_GENERATOR_MAX_SEARCH_DEPTH = "GeneratorMaxSearchDepth";
  constexpr char* OPTION_GENERATOR_KIFU_TAG = "GeneratorKifuTag";
  constexpr char* OPTION_GENERATOR_BOOK_FILE_NAME = "GeneratorStartposFileName";
  constexpr char* OPTION_GENERATOR_MIN_BOOK_MOVE = "GeneratorMinBookMove";
  constexpr char* OPTION_GENERATOR_MAX_BOOK_MOVE = "GeneratorMaxBookMove";
  constexpr char* OPTION_GENERATOR_VALUE_THRESHOLD = "GeneratorValueThreshold";
  constexpr char* OPTION_GENERATOR_MOVE_KING_TO_24_PROBABILITY = "GeneratorMoveKingTo24Probability";
  constexpr char* OPTION_GENERATOR_SWAP_TWO_PIECES_PROBABILITY = "GeneratorSwapTwoPiecesProbability";
  constexpr char* OPTION_GENERATOR_DO_RANDOM_MOVE_PROBABILITY = "GeneratorDoRandomMoveProbability";
  constexpr char* OPTION_GENERATOR_DO_RANDOM_MOVE_AFTER_BOOK = "GeneratorDoRandomMoveAfterBook";

  std::vector<std::string> book;
  std::uniform_real_distribution<> probability_distribution;

  bool ReadBook()
  {
    // 定跡ファイル(というか単なる棋譜ファイル)の読み込み
    std::string book_file_name = Options[OPTION_GENERATOR_BOOK_FILE_NAME];
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

  // 玉が2回移動する指し手
  // https://github.com/HiraokaTakuya/apery/blob/master/src/usi.cpp
  void MoveKingTo24(Position& pos, std::mt19937_64& mt, Search::StateStackPtr& state_stack) {
    Move moves[24];
    int num_moves = 0;
    for (Rank delta_rank = static_cast<Rank>(-2); delta_rank <= 2; ++delta_rank) {
      for (File delta_file = static_cast<File>(-2); delta_file <= 2; ++delta_file) {
        if (delta_rank == 0 && delta_file == 0) {
          continue;
        }
        Color us = pos.side_to_move();
        Color them = ~us;
        Square king_square = pos.king_square(us);
        Rank king_rank = rank_of(king_square);
        File king_file = file_of(king_square);

        Rank new_king_rank = king_rank + delta_rank;
        if (!is_ok(new_king_rank)) {
          continue;
        }

        File new_king_file = king_file + delta_file;
        if (!is_ok(new_king_file)) {
          continue;
        }

        // 移動先に相手の駒が利いている場合は何もしない
        Square new_king_sqaure = new_king_file | new_king_rank;
        if (pos.effected_to(them, new_king_sqaure)) {
          continue;
        }
        if (pos.pieces() & new_king_sqaure) {
          continue;
        }
        Move move = make_move(king_square, new_king_sqaure);
        moves[num_moves++] = move;
      }
    }

    if (num_moves == 0) {
      return;
    }

    Move move = moves[std::uniform_int_distribution<>(0, num_moves - 1)(mt)];
    state_stack->push(StateInfo());
    pos.do_move(move, state_stack->top());
  }

  // 2駒をランダムに入れ替える
  // https://github.com/yaneurao/YaneuraOu/blob/master/source/learn/learner.cpp
  void SwapTwoPieces(Position& pos, std::mt19937_64& mt) {
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
        // 検証用のassert
        if (!is_ok(pos)) {
          sync_cout << pos << sq1 << sq2 << sync_endl;
        }

        break;
      }
    }
  }

  // 合法手の中からランダムに1手指す
  //https://github.com/yaneurao/YaneuraOu/blob/master/source/learn/learner.cpp
  void DoRandomMove(Position& pos, std::mt19937_64& mt, Search::StateStackPtr& state_stack) {
    // 合法手のなかからランダムに1手選ぶフェーズ
    MoveList<LEGAL> list(pos);
    Move m = list.at(std::uniform_int_distribution<>(0, static_cast<int>(list.size()) - 1)(mt));

    // これが玉の移動であれば、1/2の確率で再度玉を移動させて、なるべく色んな位置の玉に対するデータを集める。
    // ただし王手がかかっている局面だと手抜くと玉を取られてしまうのでNG
    bool king_move = type_of(pos.moved_piece_after(m)) == KING;
    state_stack->push(StateInfo());
    pos.do_move(m, state_stack->top());

    if (king_move && !pos.in_check() && (mt() & 1)) {
      // 手番を変更する。
      state_stack->push(StateInfo());
      pos.do_null_move(state_stack->top());

      // 再度、玉の移動する指し手をランダムに選ぶ
      MoveList<LEGAL> list(pos);
      ExtMove* it2 = (ExtMove*)list.begin();
      for (ExtMove* it = (ExtMove*)list.begin(); it != list.end(); ++it)
        if (type_of(pos.moved_piece_after(it->move)) == KING)
          *it2++ = *it;

      auto size = it2 - list.begin();
      if (size == 0) {
        // 局面を戻しておく。
        pos.undo_null_move();
      }
      else {
        // ランダムにひとつ選ぶ
        state_stack->push(StateInfo());
        pos.do_move(list.at(std::uniform_int_distribution<>(0, static_cast<int>(size) - 1)(mt)),
          state_stack->top());
      }
    }
  }
}

void Learner::InitializeGenerator(USI::OptionsMap& o) {
  o[OPTION_GENERATOR_NUM_POSITIONS] << Option("10000000000");
  o[OPTION_GENERATOR_MIN_SEARCH_DEPTH] << Option(3, 1, MAX_PLY);
  o[OPTION_GENERATOR_MAX_SEARCH_DEPTH] << Option(4, 1, MAX_PLY);
  o[OPTION_GENERATOR_KIFU_TAG] << Option("default_tag");
  o[OPTION_GENERATOR_BOOK_FILE_NAME] << Option("startpos.sfen");
  o[OPTION_GENERATOR_MIN_BOOK_MOVE] << Option(0, 1, MAX_PLY);
  o[OPTION_GENERATOR_MAX_BOOK_MOVE] << Option(32, 1, MAX_PLY);
  o[OPTION_GENERATOR_VALUE_THRESHOLD] << Option(VALUE_MATE, 0, VALUE_MATE);
  o[OPTION_GENERATOR_MOVE_KING_TO_24_PROBABILITY] << Option("0.1");
  o[OPTION_GENERATOR_SWAP_TWO_PIECES_PROBABILITY] << Option("0.1");
  o[OPTION_GENERATOR_DO_RANDOM_MOVE_PROBABILITY] << Option("0.1");
  o[OPTION_GENERATOR_DO_RANDOM_MOVE_AFTER_BOOK] << Option(true);
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

  int min_search_depth = Options[OPTION_GENERATOR_MIN_SEARCH_DEPTH];
  int max_search_depth = Options[OPTION_GENERATOR_MAX_SEARCH_DEPTH];
  std::uniform_int_distribution<> search_depth_distribution(min_search_depth, max_search_depth);
  int min_book_move = Options[OPTION_GENERATOR_MIN_BOOK_MOVE];
  int max_book_move = Options[OPTION_GENERATOR_MAX_BOOK_MOVE];
  std::uniform_int_distribution<> num_book_move_distribution(min_book_move, max_book_move);
  int64_t num_positions = ParseOptionOrDie<int64_t>(OPTION_GENERATOR_NUM_POSITIONS);
  double swap_two_pieces_probability =
    ParseOptionOrDie<double>(OPTION_GENERATOR_SWAP_TWO_PIECES_PROBABILITY);
  double move_king_to_24_probability =
    ParseOptionOrDie<double>(OPTION_GENERATOR_MOVE_KING_TO_24_PROBABILITY);
  double do_random_move_probability =
    ParseOptionOrDie<double>(OPTION_GENERATOR_DO_RANDOM_MOVE_PROBABILITY);
  bool do_random_move_after_book = (bool)Options[OPTION_GENERATOR_DO_RANDOM_MOVE_AFTER_BOOK];
  int value_threshold = Options[OPTION_GENERATOR_VALUE_THRESHOLD];

  time_t start;
  std::time(&start);
  ASSERT_LV3(book.size());
  std::uniform_int<> opening_index(0, static_cast<int>(book.size() - 1));
  // スレッド間で共有する
  std::atomic_int64_t global_position_index = 0;
#pragma omp parallel
  {
    int thread_index = ::omp_get_thread_num();
    char output_file_path[1024];
    std::string output_file_name_tag = Options[OPTION_GENERATOR_KIFU_TAG];
    std::sprintf(output_file_path, "%s/kifu.%s.%d-%d.%I64d.%03d.bin", kifu_directory.c_str(),
      output_file_name_tag.c_str(), min_search_depth, max_search_depth, num_positions,
      thread_index);
    // 各スレッドに持たせる
    std::unique_ptr<Learner::KifuWriter> kifu_writer =
      std::make_unique<Learner::KifuWriter>(output_file_path);
    std::mt19937_64 mt19937_64(start + thread_index);

    while (global_position_index < num_positions) {
      Thread& thread = *Threads[thread_index];
      Position& pos = thread.rootPos;
      pos.set_hirate();
      auto SetupStates = Search::StateStackPtr(new aligned_stack<StateInfo>);

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

        SetupStates->push(StateInfo());
        pos.do_move(m, SetupStates->top());
      }

      if (do_random_move_after_book) {
        // 開始局面からランダムに手を指して、局面をバラけさせる
        DoRandomMove(pos, mt19937_64, SetupStates);
      }

      while (pos.game_ply() < kMaxGamePlay) {
        pos.set_this_thread(&thread);
        if (pos.is_mated()) {
          break;
        }
        int search_depth = search_depth_distribution(mt19937_64);
        auto valueAndPv = Learner::search(pos, -VALUE_INFINITE, VALUE_INFINITE, search_depth);

        // Aperyでは後手番でもスコアの値を反転させずに学習に用いている
        Value value = valueAndPv.first;
        const std::vector<Move>& pv = valueAndPv.second;
        if (pv.empty()) {
          break;
        }

        // 評価値の絶対値の上限を超えている場合は書き出さないようにする
        if (std::abs(value) > value_threshold) {
          break;
        }

        // 局面が不正な場合があるので再度チェックする
        if (pos.pos_is_ok()) {
          Learner::Record record = { 0 };
          pos.sfen_pack(record.packed);

          record.value = value;

          int num_pv = search_depth;
          num_pv = std::min<int>(num_pv, sizeof(record.pv) / sizeof(record.pv[0]));
          num_pv = std::min<int>(num_pv, static_cast<int>(pv.size()));
          copy(pv.begin(), pv.begin() + num_pv, record.pv);

          kifu_writer->Write(record);
          int64_t position_index = global_position_index++;
          ShowProgress(start, position_index, num_positions, 1000'0000);
        }

        double r = probability_distribution(mt19937_64);
        if (r < move_king_to_24_probability) {
          if (!pos.in_check()) {
            MoveKingTo24(pos, mt19937_64, SetupStates);
          }
        }
        else if (r < move_king_to_24_probability + swap_two_pieces_probability) {
          if (!pos.in_check() && pos.pieces(pos.side_to_move()).pop_count() >= 6) {
            SwapTwoPieces(pos, mt19937_64);
          }
        }
        else if (r < move_king_to_24_probability + swap_two_pieces_probability +
          do_random_move_probability) {
          if (!pos.in_check()) {
            DoRandomMove(pos, mt19937_64, SetupStates);
          }
        }
        else {
          SetupStates->push(StateInfo());
          pos.do_move(pv[0], SetupStates->top());
        }
      }
    }
  }
}
