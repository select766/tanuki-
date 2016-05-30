#include "learner.h"

#include <array>
#include <fstream>

#include "position.h"
#include "search.h"
#include "thread.h"

namespace KifuGenerator
{
  enum NodeType { PV, NonPV };
  template <NodeType NT, bool InCheck>
  Value qsearch(Position& pos, Search::Stack* ss, Value alpha, Value beta, Depth depth);
}
namespace Eval
{
  typedef std::array<int16_t, 2> ValueKpp;
  typedef std::array<int32_t, 2> ValueKkp;
  typedef std::array<int32_t, 2> ValueKk;
  // FV_SCALEで割る前の値が入る
  extern ValueKpp kpp[SQ_NB][fe_end][fe_end];
  extern ValueKkp kkp[SQ_NB][SQ_NB][fe_end];
  extern ValueKk kk[SQ_NB][SQ_NB];
  extern const int FV_SCALE = 32;

  void save_eval();
}

namespace
{
  struct PositionAndValue
  {
    std::string sfen;
    Value value;
  };

  constexpr char* KIFU_FILE_NAME = "kifu.2016-05-26.csv";
  constexpr Value CLOSE_OUT_VALUE_THRESHOLD = Value(2000);
  constexpr int POSITION_BATCH_SIZE = 1000000;
  constexpr int FV_SCALE = 32;
  constexpr float EPS = 1e-8;
  constexpr float ADAM_BETA1 = 0.9f;
  constexpr float ADAM_BETA2 = 0.999f;
  constexpr float LEARNING_RATE = 0.001f;
  constexpr int MAX_GAME_PLAY = 256;

  std::ifstream kifu_file_stream;
  std::vector<PositionAndValue> position_and_values;
  int position_and_value_index = 0;


  bool open_kifu()
  {
    kifu_file_stream.open(KIFU_FILE_NAME);
    return kifu_file_stream.is_open();
  }

  bool read_position_and_value(Position& pos, Value& value)
  {
    if (static_cast<int>(position_and_values.size()) <= position_and_value_index) {
      position_and_values.clear();
      position_and_value_index = 0;

      std::string sfen;
      while (static_cast<int>(position_and_values.size()) < POSITION_BATCH_SIZE &&
        std::getline(kifu_file_stream, sfen, ',')) {
        int value;
        kifu_file_stream >> value;
        std::string _;
        std::getline(kifu_file_stream, _);

        if (abs(value) > CLOSE_OUT_VALUE_THRESHOLD) {
          continue;
        }

        PositionAndValue position_and_value = { sfen, static_cast<Value>(value) };
        position_and_values.push_back(position_and_value);
      }

      if (position_and_values.empty()) {
        return false;
      }

      std::random_shuffle(position_and_values.begin(), position_and_values.end());
    }

    pos.set(position_and_values[position_and_value_index].sfen);
    value = position_and_values[position_and_value_index].value;
    ++position_and_value_index;
    return true;
  }

  //template<typename T>
  //std::array<T, 2> to_ints(const std::array<float, 2>& value)
  //{
  //  return{ { static_cast<T>(std::round(value[0])), static_cast<T>(std::round(value[1])) } };
  //}

  //template<typename T>
  //std::array<float, 2> to_floats(const std::array<T, 2>& value)
  //{
  //  return{ { static_cast<float>(value[0]), static_cast<float>(value[1]) } };
  //}

  int kpp_index_to_raw_index(Square k, Eval::BonaPiece p0, Eval::BonaPiece p1, Color c) {
    return static_cast<int>(static_cast<int>(static_cast<int>(k) * Eval::fe_end + p0) * Eval::fe_end + p1) * COLOR_NB + c;
  }

  int kkp_index_to_raw_index(Square k0, Square k1, Eval::BonaPiece p, Color c) {
    return kpp_index_to_raw_index(SQ_NB, Eval::BONA_PIECE_ZERO, Eval::BONA_PIECE_ZERO, COLOR_ZERO) +
      static_cast<int>(static_cast<int>(static_cast<int>(k0) * SQ_NB + k1) * Eval::fe_end + p) * COLOR_NB + c;
  }

  int kk_index_to_raw_index(Square k0, Square k1, Color c) {
    return kkp_index_to_raw_index(SQ_NB, SQ_ZERO, Eval::BONA_PIECE_ZERO, COLOR_ZERO) +
      (static_cast<int>(static_cast<int>(k0) * SQ_NB + k1) * COLOR_NB) + c;
  }

  void increase_parameters(int64_t current_index, float dt, float& w, float& m, float& v, float& sum_w, int64_t& last_update_index) {
    float previous_w = w;

    // Adam
    m = ADAM_BETA1 * m + (1.0f - ADAM_BETA1) * dt;
    v = ADAM_BETA2 * v + (1.0f - ADAM_BETA2) * dt * dt;
    float t = current_index + 1.0f;
    float mm = m / (1.0f - pow(ADAM_BETA1, t));
    float vv = v / (1.0f - pow(ADAM_BETA2, t));
    float delta = LEARNING_RATE * mm / (sqrt(vv) + EPS);
    //std::cerr << current_index << " " << delta << std::endl;
    w += delta;

    // 平均化確率的勾配降下法
    sum_w += previous_w * (current_index - last_update_index);
    last_update_index = current_index;
  }
}

void Learner::learn()
{
  ASSERT_LV3(
    kk_index_to_raw_index(SQ_NB, SQ_ZERO, COLOR_ZERO) ==
    static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * static_cast<int>(Eval::fe_end) * 2 +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * 2 +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * 2);

  Eval::load_eval();

  if (!open_kifu()) {
    sync_cout << "info string Failed to open the kifu file." << sync_endl;
    return;
  }

  int vector_length = kk_index_to_raw_index(SQ_NB, SQ_ZERO, COLOR_ZERO);

  // 重みベクトル
  std::vector<float> w(vector_length);
  // Adam用変数
  std::vector<float> m(vector_length);
  // Adam用変数
  std::vector<float> v(vector_length);
  // 平均化確率的勾配降下法用変数
  std::vector<float> sum_w(vector_length);
  std::vector<int64_t> last_update_indexes(vector_length);

  for (Square k : SQ) {
    for (Eval::BonaPiece p0 = Eval::BONA_PIECE_ZERO; p0 < Eval::fe_end; ++p0) {
      for (Eval::BonaPiece p1 = Eval::BONA_PIECE_ZERO; p1 < Eval::fe_end; ++p1) {
        for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
          w[kpp_index_to_raw_index(k, p0, p1, c)] = static_cast<float>(Eval::kpp[k][p0][p1][c]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Eval::BonaPiece p = Eval::BONA_PIECE_ZERO; p < Eval::fe_end; ++p) {
        for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
          w[kkp_index_to_raw_index(k0, k1, p, c)] = static_cast<float>(Eval::kkp[k0][k1][p][c]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
        w[kk_index_to_raw_index(k0, k1, c)] = static_cast<float>(Eval::kk[k0][k1][c]);
      }
    }
  }

  Thread& thread = *Threads.main();

  Search::LimitsType limits;
  limits.max_game_ply = MAX_GAME_PLAY;
  limits.depth = 1;
  limits.silent = true;
  Search::Limits = limits;

  Position& position = thread.rootPos;
  Value recordedValue;
  int64_t current_index = 0;
  while (read_position_and_value(position, recordedValue)) {
    // 浅い探索を行う
    // 本当はqsearch()で行いたいが、直接呼んだらクラッシュしたので…。
    thread.rootMoves.clear();
    // 王手が掛かっているかどうかに応じてrootMovesの種類を変える
    if (position.in_check()) {
      for (auto m : MoveList<EVASIONS>(position)) {
        thread.rootMoves.push_back(Search::RootMove(m));
      }
    }
    else {
      for (auto m : MoveList<CAPTURES_PRO_PLUS>(position)) {
        thread.rootMoves.push_back(Search::RootMove(m));
      }
      for (auto m : MoveList<QUIET_CHECKS>(position)) {
        thread.rootMoves.push_back(Search::RootMove(m));
      }
    }
    //for (auto m : MoveList<LEGAL>(position)) {
    //  thread.rootMoves.push_back(Search::RootMove(m));
    //}

    if (thread.rootMoves.empty()) {
      // 現在の局面が静止している状態なのだと思う
      // 後段の処理で現在の局面の評価値を計算させるため
      // ダミーのRootMoveを追加する
      thread.rootMoves.push_back(Search::RootMove(MOVE_NONE));
      thread.rootMoves[0].pv.clear();
   }
    else {
      // 実際に探索する
      thread.maxPly = 0;
      thread.rootDepth = 0;
      position.set_this_thread(&thread);

      //std::cerr << pos << std::endl;

      thread.Thread::start_searching();
      thread.wait_for_search_finished();
    }

    // 静止した局面まで進める
    StateInfo stateInfo[MAX_PLY];
    int play = 0;
    Color rootColor = position.side_to_move();
    for (auto m : thread.rootMoves[0].pv) {
      position.do_move(m, stateInfo[play++]);
    }

    // 静止した局面の評価値を求める
    Value value = Eval::compute_eval(position);
    if (rootColor != position.side_to_move()) {
      value = -value;
    }
    if (rootColor == WHITE) {
      value = -value;
    }
    ASSERT_LV3(thread.rootMoves[0].score == value);

    // 深い深さの探索による評価値との差分を求める
    float delta = static_cast<float>((recordedValue - value) * FV_SCALE);

    // 値を更新する
    Square sq_bk = position.king_square(BLACK);
    Square sq_wk = position.king_square(WHITE);
    const auto& list0 = position.eval_list()->piece_list_fb();
    const auto& list1 = position.eval_list()->piece_list_fw();

    // 本当はマクロは使いたくないけど
    // マクロ無しだと冗長になって読みにくくなるので仕方がないと思う…
#define INCREASE_PARAMETERS(delta, index, eval_element) \
    increase_parameters(current_index, delta, w[index], m[index], v[index], sum_w[index], last_update_indexes[index]); \
    eval_element = static_cast<int32_t>(std::round(w[index]));

    // KK
    INCREASE_PARAMETERS(delta, kk_index_to_raw_index(sq_bk, sq_wk, BLACK), Eval::kk[sq_bk][sq_wk][BLACK]);
    INCREASE_PARAMETERS(delta, kk_index_to_raw_index(sq_bk, sq_wk, WHITE), Eval::kk[sq_bk][sq_wk][WHITE]);

    for (int i = 0; i < PIECE_NO_KING; ++i) {
      Eval::BonaPiece k0 = list0[i];
      Eval::BonaPiece k1 = list1[i];
      for (int j = 0; j < i; ++j) {
        Eval::BonaPiece l0 = list0[j];
        Eval::BonaPiece l1 = list1[j];

        // KPP
        INCREASE_PARAMETERS(delta, kpp_index_to_raw_index(sq_bk, k0, l0, BLACK), Eval::kpp[sq_bk][k0][l0][BLACK]);
        INCREASE_PARAMETERS(delta, kpp_index_to_raw_index(sq_bk, k0, l0, WHITE), Eval::kpp[sq_bk][k0][l0][WHITE]);

        // KPP
        INCREASE_PARAMETERS(-delta, kpp_index_to_raw_index(Inv(sq_wk), k1, l1, BLACK), Eval::kpp[Inv(sq_wk)][k1][l1][BLACK]);
        INCREASE_PARAMETERS(delta, kpp_index_to_raw_index(Inv(sq_wk), k1, l1, WHITE), Eval::kpp[Inv(sq_wk)][k1][l1][WHITE]);
      }

      // KKP
      INCREASE_PARAMETERS(delta, kkp_index_to_raw_index(sq_bk, sq_wk, k0, BLACK), Eval::kkp[sq_bk][sq_wk][k0][BLACK]);
      INCREASE_PARAMETERS(delta, kkp_index_to_raw_index(sq_bk, sq_wk, k0, WHITE), Eval::kkp[sq_bk][sq_wk][k0][WHITE]);
    }

#undef INCREASE_PARAMETERS

    // 前回計算した分をクリアする
    Value value_after = Eval::compute_eval(position);
    if (rootColor != position.side_to_move()) {
      value_after = -value_after;
    }
    if (rootColor == WHITE) {
      value_after = -value_after;
    }
    ASSERT_LV3(value == value_after);

    if (current_index % 1000 == 0) {
      fprintf(stderr, "recorded=%5d before=%5d after=%5d delta=%5d target=%c turn=%s\n",
        static_cast<int>(recordedValue),
        static_cast<int>(value),
        static_cast<int>(value_after),
        static_cast<int>(value_after - value),
        delta > 0 ? '+' : '-',
        rootColor == BLACK ? "black" : "white");
    }

    // 局面は元に戻さなくても大丈夫
    ++current_index;
  }

#define CLAMP_AND_STORE(index, type, dest) \
  sum_w[index] += w[index] * (current_index - last_update_indexes[index]); \
  int64_t value = static_cast<int64_t>(std::round(sum_w[index] / current_index)); \
  value = std::max<int64_t>(std::numeric_limits<type>::min(), value); \
  value = std::min<int64_t>(std::numeric_limits<type>::max(), value); \
  dest = static_cast<type>(value);

  for (Square k : SQ) {
    for (Eval::BonaPiece p0 = Eval::BONA_PIECE_ZERO; p0 < Eval::fe_end; ++p0) {
      for (Eval::BonaPiece p1 = Eval::BONA_PIECE_ZERO; p1 < Eval::fe_end; ++p1) {
        for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
          CLAMP_AND_STORE(kpp_index_to_raw_index(k, p0, p1, c), int16_t, Eval::kpp[k][p0][p1][c]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Eval::BonaPiece p = Eval::BONA_PIECE_ZERO; p < Eval::fe_end; ++p) {
        for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
          CLAMP_AND_STORE(kkp_index_to_raw_index(k0, k1, p, c), int32_t, Eval::kkp[k0][k1][p][c]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
        CLAMP_AND_STORE(kk_index_to_raw_index(k0, k1, c), int32_t, Eval::kk[k0][k1][c]);
      }
    }
  }

#undef CLAMP_AND_STORE

  Eval::save_eval();
}
