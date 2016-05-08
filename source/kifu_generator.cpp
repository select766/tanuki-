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
  constexpr int NumGames = 10000;
  constexpr char* BookFileName = "book.sfen";
  constexpr char* OutputFileName = "kifu.csv";
  constexpr int MaxBookMove = 32;
  constexpr Depth SearchDepth = Depth(3);
  constexpr Value CloseOutValueThreshold = Value(2000);
  constexpr int MaxGamePlay = 256;

  std::mutex output_mutex;
  std::ofstream output_stream;
  std::atomic_int global_game_index = 0;
  std::vector<std::string> book;
  std::random_device random_device;
  std::mt19937_64 mt19937_64(random_device());

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
    std::uniform_int<> opening_index(0, static_cast<int>(book.size()));
    for (int game_index = global_game_index++; game_index < NumGames; game_index = global_game_index++)
    {
      if (game_index && game_index % 1000 == 0) {
        auto current = std::chrono::system_clock::now();
        auto duration = current - start;
        auto remaining = duration * (NumGames - game_index) / game_index;
        int remainingSec = std::chrono::duration_cast<std::chrono::seconds>(remaining).count();
        int h = remainingSec / 3600;
        int m = remainingSec / 60 % 60;
        int s = remainingSec % 60;
        char buffer[1024];
        sprintf(buffer, "%d / %d (%02d:%02d:%02d)", game_index, NumGames, h, m, s);
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

      // TODO(nodchip): 自駒2個の入れ替えを実装する

      while (pos.game_ply() < MaxGamePlay) {
        thread.rootMoves.clear();
        for (auto m : MoveList<LEGAL>(pos)) {
          thread.rootMoves.push_back(RootMove(m));
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

  std::vector<std::thread> threads;
  while (threads.size() < Options["Threads"]) {
    int thread_id = static_cast<int>(threads.size());
    threads.push_back(std::thread([thread_id] {generate_procedure(thread_id); }));
  }
  for (auto& thread : threads) {
    thread.join();
  }
}
