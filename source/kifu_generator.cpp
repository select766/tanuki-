#include "kifu_generator.h"

#include <atomic>
#include <ctime>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <omp.h>

#include "kifu_writer.h"
#include "search.h"
#include "thread.h"

using Search::RootMove;

namespace Learner
{
  std::pair<Value, std::vector<Move> > search(Position& pos, Value alpha, Value beta, int depth);
  std::pair<Value, std::vector<Move> > qsearch(Position& pos, Value alpha, Value beta);
}

namespace
{
  constexpr int kNumGames = 10'0000;
  constexpr char* kOutputFileNameDate = "2016-07-16";
  constexpr char* kBookFileName = "book.sfen";
  constexpr int kMinBookMove = 32;
  constexpr int kMaxBookMove = 32;
  constexpr Depth kSearchDepth = Depth(3);
  constexpr int kMaxGamePlay = 256;
  constexpr int kMaxSwapTrials = 10;
  constexpr int kMaxTrialsToSelectSquares = 100;

  std::vector<std::string> book;
  std::random_device random_device;
  std::mt19937_64 mt19937_64(random_device());
  std::uniform_int_distribution<> swap_distribution(0, 9);
  std::uniform_int_distribution<> num_book_move_distribution(kMinBookMove, kMaxBookMove);

  bool ReadBook()
  {
    // 定跡ファイル(というか単なる棋譜ファイル)の読み込み
    std::ifstream fs_book;
    fs_book.open(kBookFileName);

    if (!fs_book.is_open())
    {
      sync_cout << "Error! : can't read " << kBookFileName << sync_endl;
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
}

void Learner::GenerateKifu()
{
  std::srand(std::time(nullptr));

  // 定跡の読み込み
  if (!ReadBook()) {
    return;
  }

  Eval::load_eval();

  Search::LimitsType limits;
  limits.max_game_ply = kMaxGamePlay;
  limits.depth = kSearchDepth;
  limits.silent = true;
  Search::Limits = limits;

  auto start = std::chrono::system_clock::now();
  ASSERT_LV3(book.size());
  std::uniform_int<> opening_index(0, static_cast<int>(book.size() - 1));
  // スレッド間で共有する
  std::atomic_int global_game_index = 0;
#pragma omp parallel
  {
    int thread_index = ::omp_get_thread_num();
    char output_file_path[1024];
    std::sprintf(output_file_path, "%s/kifu.%s.%d.%d.%03d.bin",
      ((std::string)Options["KifuDir"]).c_str(), kOutputFileNameDate, kSearchDepth, kNumGames,
      thread_index);
    // 各スレッドに持たせる
    std::unique_ptr<Learner::KifuWriter> kifu_writer =
      std::make_unique<Learner::KifuWriter>(output_file_path);

#pragma omp for schedule(guided)
    for (int game_index = 0; game_index < kNumGames; ++game_index) {
      int current_game_index = global_game_index++;
      if (current_game_index && current_game_index % 1000 == 0) {
        auto current = std::chrono::system_clock::now();
        auto elapsed = current - start;
        double elapsed_sec =
          static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
        int remaining_sec = static_cast<int>(elapsed_sec / current_game_index * (kNumGames - current_game_index));
        int h = remaining_sec / 3600;
        int m = remaining_sec / 60 % 60;
        int s = remaining_sec % 60;

        time_t     current_time;
        struct tm  *local_time;

        time(&current_time);
        local_time = localtime(&current_time);
        std::printf("%d / %d (%04d-%02d-%02d %02d:%02d:%02d remaining %02d:%02d:%02d)\n",
          current_game_index, kNumGames,
          local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
          local_time->tm_hour, local_time->tm_min, local_time->tm_sec, h, m, s);
        std::fflush(stdout);
      }

      Thread& thread = *Threads[thread_index];
      Position& pos = thread.rootPos;
      pos.set_hirate();
      auto SetupStates = Search::StateStackPtr(new aligned_stack<StateInfo>);

      const std::string& opening = book[opening_index(mt19937_64)];
      std::istringstream is(opening);
      std::string token;
      int num_book_move = num_book_move_distribution(mt19937_64);
      while (pos.game_ply() < num_book_move)
      {
        is >> token;
        if (token == "startpos" || token == "moves")
          continue;

        Move m = move_from_usi(pos, token);
        pos.check_info_update();
        if (!is_ok(m) || !pos.legal(m))
        {
          //  sync_cout << "Error book.sfen , line = " << book_number << " , moves = " << token << endl << rootPos << sync_endl;
          // →　エラー扱いはしない。
          break;
        }

        SetupStates->push(StateInfo());
        pos.do_move(m, SetupStates->top());
      }

      while (pos.game_ply() < kMaxGamePlay) {
        // 一定の確率で自駒2駒を入れ替える
        // TODO(tanuki-): 前提条件が絞り込めていないので絞り込む
        if (swap_distribution(mt19937_64) == 0 &&
          pos.pos_is_ok() &&
          !pos.is_mated() &&
          !pos.in_check() &&
          !pos.attackers_to(pos.side_to_move(), pos.king_square(~pos.side_to_move()))) {
          std::string originalSfen = pos.sfen();
          int counter = 0;
          for (; counter < kMaxSwapTrials; ++counter) {
            pos.set(originalSfen);

            // 自駒2駒を入れ替える
            std::vector<Square> myPieceSquares;
            for (Square square = SQ_11; square < SQ_NB; ++square) {
              Piece piece = pos.piece_on(square);
              if (piece == NO_PIECE) {
                continue;
              }
              else if (color_of(piece) == pos.side_to_move()) {
                myPieceSquares.push_back(square);
              }
            }

            // 一時領域として使用するマスを選ぶ
            Square square0;
            Square square1;
            int num_trials_to_select_squares;
            // 無限ループに陥る場合があるので制限をかける
            for (num_trials_to_select_squares = 0;
              num_trials_to_select_squares < kMaxTrialsToSelectSquares;
              ++num_trials_to_select_squares) {
              std::uniform_int_distribution<> square_index_distribution(0, myPieceSquares.size() - 1);
              square0 = myPieceSquares[square_index_distribution(mt19937_64)];
              square1 = myPieceSquares[square_index_distribution(mt19937_64)];
              // 同じ種類の駒を選ばないようにする
              if (pos.piece_on(square0) != pos.piece_on(square1)) {
                break;
              }
            }

            if (num_trials_to_select_squares == kMaxTrialsToSelectSquares) {
              break;
            }

            Piece piece0 = pos.piece_on(square0);
            PieceNo pieceNo0 = pos.piece_no_of(piece0, square0);
            Piece piece1 = pos.piece_on(square1);
            PieceNo pieceNo1 = pos.piece_no_of(piece1, square1);

            // 玉が相手の駒の利きのある升に移動する場合はやり直す
            // TODO(tanuki-): 必要かどうかわからないので調べる
            if (type_of(piece0) == KING && pos.attackers_to(~pos.side_to_move(), square1)) {
              continue;
            }
            if (type_of(piece1) == KING && pos.attackers_to(~pos.side_to_move(), square0)) {
              continue;
            }

            // 2つの駒を入れ替える
            pos.remove_piece(square0);
            pos.remove_piece(square1);
            pos.put_piece(square0, piece1, pieceNo1);
            pos.put_piece(square1, piece0, pieceNo0);

            // 不正な局面になった場合はやり直す
            // ランダムに入れ替えると2歩・9段目の歩香・89段目の桂など
            // 不正な状態になる場合があるため
            if (!pos.pos_is_ok()) {
              continue;
            }

            // 1手詰めになった場合もやり直す
            // TODO(tanuki-): やり直しの条件を絞り込む
            if (pos.mate1ply()) {
              continue;
            }

            // 詰んでしまっている場合もやり直す
            // TODO(tanuki-): やり直しの条件を絞り込む
            if (pos.is_mated()) {
              continue;
            }

            // 王手がかかっている場合もキャンセル
            if (pos.in_check()) {
              continue;
            }

            // 入れ替えた駒で相手の王を取れる場合もやり直す
            if (pos.attackers_to(pos.side_to_move(), pos.king_square(~pos.side_to_move()))) {
              continue;
            }

            // ハッシュ・利き等の内部状態を更新するためsfen化してsetする
            pos.set(pos.sfen());

            break;
          };

          // 何度やり直してもうまくいかない場合は諦める
          if (counter >= kMaxSwapTrials) {
            pos.set(originalSfen);
          }
        }

        pos.set_this_thread(&thread);
        if (pos.is_mated()) {
          break;
        }
        auto valueAndPv = Learner::search(pos, -VALUE_INFINITE, VALUE_INFINITE, kSearchDepth);

        // Aperyでは後手番でもスコアの値を反転させずに学習に用いている
        Value value = valueAndPv.first;
        if (value < -VALUE_MATE) {
          // 詰みの局面なので書き出さない
          break;
        }

        const std::vector<Move>& pv = valueAndPv.second;
        if (pv.empty()) {
          break;
        }

        // 局面が不正な場合があるので再度チェックする
        if (pos.pos_is_ok()) {
          Learner::Record record;
          pos.sfen_pack(record.packed);
          record.value = value;
          kifu_writer->Write(record);
        }

        SetupStates->push(StateInfo());
        pos.do_move(pv[0], SetupStates->top());
      }
    }
  }
}
