#include "kifu_generator.h"

#include <atomic>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>

#include "search.h"
#include "thread.h"

using Search::RootMove;

namespace
{
  constexpr int NumGames = 1000000;
  constexpr char* BookFileName = "book.sfen";
  constexpr char* OutputFileName = "kifu.csv";
  constexpr int MaxBookMove = 32;
  constexpr Depth SearchDepth = Depth(3);
  constexpr Value CloseOutValueThreshold = Value(2000);
  constexpr int MaxGamePlay = 256;
  constexpr int MaxSwapTrials = 10;

  std::mutex output_mutex;
  std::ofstream output_stream;
  std::atomic_int global_game_index = 0;
  std::vector<std::string> book;
  std::random_device random_device;
  std::mt19937_64 mt19937_64(random_device());
  std::uniform_int_distribution<> swap_distribution(0, 9);

  bool read_book()
  {
    // 定跡ファイル(というか単なる棋譜ファイル)の読み込み
    std::ifstream fs_book;
    fs_book.open(BookFileName);

    if (!fs_book.is_open())
    {
      sync_cout << "Error! : can't read " << BookFileName << sync_endl;
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

  void generate_procedure(int thread_id)
  {
    auto start = std::chrono::system_clock::now();
    ASSERT_LV3(book.size());
    std::uniform_int<> opening_index(0, static_cast<int>(book.size() - 1));
    for (int game_index = global_game_index++; game_index < NumGames; game_index = global_game_index++)
    {
      if (game_index && game_index % 100 == 0) {
        auto current = std::chrono::system_clock::now();
        auto duration = current - start;
        auto remaining = duration * (NumGames - game_index) / game_index;
        int remainingSec = std::chrono::duration_cast<std::chrono::seconds>(remaining).count();
        int h = remainingSec / 3600;
        int m = remainingSec / 60 % 60;
        int s = remainingSec % 60;

        time_t     current_time;
        struct tm  *local_time;

        time(&current_time);
        local_time = localtime(&current_time);
        char buffer[1024];
        sprintf(buffer, "%d / %d (%04d-%02d-%02d %02d:%02d:%02d remaining %02d:%02d:%02d)",
          game_index, NumGames,
          local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
          local_time->tm_hour, local_time->tm_min, local_time->tm_sec, h, m, s);
        std::cerr << buffer << std::endl;
      }

      Thread& thread = *Threads[thread_id];
      Position& pos = thread.rootPos;
      pos.set_hirate();
      auto SetupStates = Search::StateStackPtr(new aligned_stack<StateInfo>);

      const std::string& opening = book[opening_index(mt19937_64)];
      std::istringstream is(opening);
      std::string token;
      while (pos.game_ply() < MaxBookMove)
      {
        is >> token;
        if (token == "startpos" || token == "moves")
          continue;

        Move m = move_from_usi(pos, token);
        if (!is_ok(m))
        {
          //  sync_cout << "Error book.sfen , line = " << book_number << " , moves = " << token << endl << rootPos << sync_endl;
          // →　エラー扱いはしない。
          break;
        }
        else {
          SetupStates->push(StateInfo());
          pos.do_move(m, SetupStates->top());
        }
      }

      while (pos.game_ply() < MaxGamePlay) {
        if (swap_distribution(mt19937_64) == 0 && pos.pos_is_ok() && !pos.mate1ply() && !pos.is_mated() && !pos.in_check()) {
          std::string originalSfen = pos.sfen();
          int counter = 0;
          for (; counter < MaxSwapTrials; ++counter) {
            pos.set(originalSfen);

            // 自駒2駒を入れ替える
            std::vector<Square> myPieceSquares;
            std::vector<Square> emptySquares;
            for (Square square = SQ_11; square < SQ_NB; ++square) {
              Piece piece = pos.piece_on(square);
              if (piece == NO_PIECE) {
                emptySquares.push_back(square);
              } else if (color_of(piece) == pos.side_to_move()) {
                myPieceSquares.push_back(square);
              }
            }

            std::random_shuffle(emptySquares.begin(), emptySquares.end());
            Square emptySquare0 = emptySquares[0];
            Square emptySquare1 = emptySquares[1];

            Square square0;
            Square square1;
            do {
              std::uniform_int_distribution<> square_index_distribution(0, myPieceSquares.size() - 1);
              square0 = myPieceSquares[square_index_distribution(mt19937_64)];
              square1 = myPieceSquares[square_index_distribution(mt19937_64)];
            } while (pos.piece_on(square0) == pos.piece_on(square1));

            Piece piece0 = pos.piece_on(square0);
            Piece piece1 = pos.piece_on(square1);

            // 玉が相手の駒の効きのある升に移動しそうな場合はキャンセル
            // TODO(tanuki-): 必要かどうかわからないので調べる
            if (type_of(piece0) == KING && pos.attackers_to(~pos.side_to_move(), square1)) {
              continue;
            }
            if (type_of(piece1) == KING && pos.attackers_to(~pos.side_to_move(), square0)) {
              continue;
            }

            StateInfo stateInfo[4];
            pos.do_move(make_move(square0, emptySquare0), stateInfo[0]);
            pos.do_move(make_move(square1, square0), stateInfo[1]);
            pos.do_move(make_move(emptySquare0, emptySquare1), stateInfo[2]);
            pos.do_move(make_move(emptySquare1, square1), stateInfo[3]);

            // 不正な局面になった場合はキャンセル
            // ランダムに入れ替えると2歩等不正な状態になる場合があるため
            if (!pos.pos_is_ok()) {
              continue;
            }

            // 1手詰めになった場合もキャンセル
            // TODO(tanuki-): 必要かどうかわからないので調べる
            if (pos.mate1ply()) {
              continue;
            }

            // 詰んでしまっている場合もキャンセル
            // TODO(tanuki-): 必要かどうかわからないので調べる
            if (pos.is_mated()) {
              continue;
            }

            // 王手がかかっている場合もキャンセル
            if (pos.in_check()) {
              continue;
            }

            // 入れ替えた駒で相手の王を取れる場合もキャンセル
            if (pos.attackers_to(pos.side_to_move(), pos.king_square(~pos.side_to_move()))) {
              continue;
            }

            //std::cerr << pos << std::endl;

            pos.set(pos.sfen());

            break;
          };

          if (counter >= MaxSwapTrials) {
            pos.set(originalSfen);
            //std::cerr << pos << std::endl;
          }
        }

        thread.rootMoves.clear();
        bool mateExist = false;
        for (auto m : MoveList<LEGAL>(pos)) {
          // ランダムに入れ替えるといきなり相手玉を取れるような配置になる場合があるため
          if (type_of(pos.piece_on(move_to(m))) == KING) {
            mateExist = true;
            break;
          }
          thread.rootMoves.push_back(RootMove(m));
        }

        if (mateExist) {
          break;
        }

        if (thread.rootMoves.empty()) {
          break;
        }
        thread.maxPly = 0;
        thread.rootDepth = 0;
        pos.set_this_thread(&thread);

        //std::cerr << pos << std::endl;

        thread.Thread::start_searching();
        thread.wait_for_search_finished();

        int score = thread.rootMoves[0].score;
        if (pos.side_to_move() == WHITE) {
          score = -score;
        }

        {
          std::lock_guard<std::mutex> lock(output_mutex);
          output_stream << pos.sfen() << "," << score << std::endl;
        }

        SetupStates->push(StateInfo());
        pos.do_move(thread.rootMoves[0].pv[0], SetupStates->top());
      }
    }
  }
}

void KifuGenerator::generate()
{
  // 定跡の読み込み
  if (!read_book()) {
    return;
  }

  // 出力ファイルを開く
  output_stream.open(OutputFileName);
  if (!output_stream.is_open()) {
    sync_cout << "Error! : can't open " << OutputFileName << sync_endl;
    return;
  }

  Search::LimitsType limits;
  limits.max_game_ply = MaxGamePlay;
  limits.depth = SearchDepth;
  limits.silent = true;
  Search::Limits = limits;

  //generate_procedure(0);
  //Options["Threads"] = 2;

  std::vector<std::thread> threads;
  while (threads.size() < Options["Threads"]) {
    int thread_id = static_cast<int>(threads.size());
    threads.push_back(std::thread([thread_id] {generate_procedure(thread_id); }));
  }
  for (auto& thread : threads) {
    thread.join();
  }
}
