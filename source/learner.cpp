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
  using VALUE_TYPE = double;

  struct PositionAndValue
  {
    std::string sfen;
    Value value;
  };

  constexpr char* KIFU_FILE_NAME = "kifu/kifu.2016-05-30.100000.csv";
  constexpr Value CLOSE_OUT_VALUE_THRESHOLD = Value(2000);
  constexpr int POSITION_BATCH_SIZE = 1000000;
  constexpr int FV_SCALE = 32;
  constexpr VALUE_TYPE EPS = 1e-8;
  constexpr VALUE_TYPE ADAM_BETA1 = 0.9;
  constexpr VALUE_TYPE ADAM_BETA2 = 0.999;
  constexpr VALUE_TYPE LEARNING_RATE = 0.001;
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

  void increase_parameters(int64_t current_index, VALUE_TYPE dt, VALUE_TYPE& w, VALUE_TYPE& m, VALUE_TYPE& v, VALUE_TYPE& sum_w, int64_t& last_update_index) {
    VALUE_TYPE previous_w = w;

    // Adam
    m = ADAM_BETA1 * m + (1.0 - ADAM_BETA1) * dt;
    v = ADAM_BETA2 * v + (1.0 - ADAM_BETA2) * dt * dt;
    VALUE_TYPE t = current_index + 1.0;
    VALUE_TYPE mm = m / (1.0 - pow(ADAM_BETA1, t));
    VALUE_TYPE vv = v / (1.0 - pow(ADAM_BETA2, t));
    VALUE_TYPE delta = LEARNING_RATE * mm / (sqrt(vv) + EPS);
    //std::cerr << current_index << " " << delta << std::endl;
    w += delta;

    // 平均化確率的勾配降下法
    sum_w += previous_w * (current_index - last_update_index);
    last_update_index = current_index;
  }

  template<typename T>
  void increase_parameters(int64_t current_index, VALUE_TYPE delta, int feature_index, T& eval_element, std::vector<VALUE_TYPE>& w, std::vector<VALUE_TYPE>& m, std::vector<VALUE_TYPE>& v, std::vector<VALUE_TYPE>& sum_w, std::vector<int64_t>& last_update_indexes) {
    increase_parameters(current_index, delta, w[feature_index], m[feature_index], v[feature_index], sum_w[feature_index], last_update_indexes[feature_index]);
    eval_element = static_cast<T>(std::round(w[feature_index]));
  }

  template<typename T>
  void clamp_and_store(int64_t current_index, int feature_index, T& dest, std::vector<VALUE_TYPE>& sum_w, std::vector<VALUE_TYPE>& w, std::vector<int64_t>& last_update_indexes)
  {
    sum_w[feature_index] += w[feature_index] * (current_index - last_update_indexes[feature_index]);
    int64_t value = static_cast<int64_t>(std::round(sum_w[feature_index] / current_index));
    value = std::max<int64_t>(std::numeric_limits<T>::min(), value);
    value = std::min<int64_t>(std::numeric_limits<T>::max(), value);
    dest = static_cast<T>(value);
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
  std::vector<VALUE_TYPE> w(vector_length);
  // Adam用変数
  std::vector<VALUE_TYPE> m(vector_length);
  // Adam用変数
  std::vector<VALUE_TYPE> v(vector_length);
  // 平均化確率的勾配降下法用変数
  std::vector<VALUE_TYPE> sum_w(vector_length);
  std::vector<int64_t> last_update_indexes(vector_length);

  for (Square k : SQ) {
    for (Eval::BonaPiece p0 = Eval::BONA_PIECE_ZERO; p0 < Eval::fe_end; ++p0) {
      for (Eval::BonaPiece p1 = Eval::BONA_PIECE_ZERO; p1 < Eval::fe_end; ++p1) {
        for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
          w[kpp_index_to_raw_index(k, p0, p1, c)] = static_cast<VALUE_TYPE>(Eval::kpp[k][p0][p1][c]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Eval::BonaPiece p = Eval::BONA_PIECE_ZERO; p < Eval::fe_end; ++p) {
        for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
          w[kkp_index_to_raw_index(k0, k1, p, c)] = static_cast<VALUE_TYPE>(Eval::kkp[k0][k1][p][c]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
        w[kk_index_to_raw_index(k0, k1, c)] = static_cast<VALUE_TYPE>(Eval::kk[k0][k1][c]);
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
  Value recordValue;
  int64_t current_index = 0;
  while (read_position_and_value(position, recordValue)) {
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

    Value value;
    if (thread.rootMoves.empty()) {
      // 現在の局面が静止している状態なのだと思う
      // 現在の局面の評価値をそのまま使う
      value = Eval::compute_eval(position);
    }
    else {
      // 実際に探索する
      thread.maxPly = 0;
      thread.rootDepth = 0;
      position.set_this_thread(&thread);

      //std::cerr << pos << std::endl;

      thread.Thread::start_searching();
      thread.wait_for_search_finished();
      value = thread.rootMoves[0].score;
    }

    Color rootColor = position.side_to_move();

    // 静止した局面まで進める
    StateInfo stateInfo[MAX_PLY];
    int play = 0;
    for (auto m : thread.rootMoves[0].pv) {
      position.do_move(m, stateInfo[play++]);
    }

    // 深い深さの探索による評価値との差分を求める
    VALUE_TYPE delta = static_cast<VALUE_TYPE>((recordValue - value) * FV_SCALE);
    // Aperyだと[0]に相当する値は現在の局面の手番で場合分けしている
    VALUE_TYPE delta0 = rootColor == BLACK ? delta : -delta;
    // Aperyだと[1]に相当する値は現在の局面の手番と
    // 浅い探索のPVの末端の局面の手番が等しいかどうかで場合分けしている
    // TODO(tanuki-): 理由を考える
    VALUE_TYPE delta1 = rootColor == position.side_to_move() ? delta : -delta;

    // 値を更新する
    Square sq_bk = position.king_square(BLACK);
    Square sq_wk = position.king_square(WHITE);
    const auto& list0 = position.eval_list()->piece_list_fb();
    const auto& list1 = position.eval_list()->piece_list_fw();

    // KK
    increase_parameters(current_index, delta0, kk_index_to_raw_index(sq_bk, sq_wk, BLACK), Eval::kk[sq_bk][sq_wk][BLACK], w, m, v, sum_w, last_update_indexes);
    increase_parameters(current_index, delta1, kk_index_to_raw_index(sq_bk, sq_wk, WHITE), Eval::kk[sq_bk][sq_wk][WHITE], w, m, v, sum_w, last_update_indexes);

    for (int i = 0; i < PIECE_NO_KING; ++i) {
      Eval::BonaPiece k0 = list0[i];
      Eval::BonaPiece k1 = list1[i];
      for (int j = 0; j < i; ++j) {
        Eval::BonaPiece l0 = list0[j];
        Eval::BonaPiece l1 = list1[j];

        // KPP
        increase_parameters(current_index, delta0, kpp_index_to_raw_index(sq_bk, k0, l0, BLACK), Eval::kpp[sq_bk][k0][l0][BLACK], w, m, v, sum_w, last_update_indexes);
        increase_parameters(current_index, delta1, kpp_index_to_raw_index(sq_bk, k0, l0, WHITE), Eval::kpp[sq_bk][k0][l0][WHITE], w, m, v, sum_w, last_update_indexes);

        // KPP
        increase_parameters(current_index, -delta0, kpp_index_to_raw_index(Inv(sq_wk), k1, l1, BLACK), Eval::kpp[Inv(sq_wk)][k1][l1][BLACK], w, m, v, sum_w, last_update_indexes);
        increase_parameters(current_index, delta1, kpp_index_to_raw_index(Inv(sq_wk), k1, l1, WHITE), Eval::kpp[Inv(sq_wk)][k1][l1][WHITE], w, m, v, sum_w, last_update_indexes);
      }

      // KKP
      increase_parameters(current_index, delta0, kkp_index_to_raw_index(sq_bk, sq_wk, k0, BLACK), Eval::kkp[sq_bk][sq_wk][k0][BLACK], w, m, v, sum_w, last_update_indexes);
      increase_parameters(current_index, delta1, kkp_index_to_raw_index(sq_bk, sq_wk, k0, WHITE), Eval::kkp[sq_bk][sq_wk][k0][WHITE], w, m, v, sum_w, last_update_indexes);
    }

    if (current_index % 1000 == 0) {
      Value value_after = Eval::compute_eval(position);
      if (rootColor != position.side_to_move()) {
        value_after = -value_after;
      }

      fprintf(stderr, "recorded=%5d before=%5d after=%5d delta=%5d target=%c turn=%s\n",
        static_cast<int>(recordValue),
        static_cast<int>(value),
        static_cast<int>(value_after),
        static_cast<int>(value_after - value),
        delta > 0 ? '+' : '-',
        rootColor == BLACK ? "black" : "white");
    }

    // 局面は元に戻さなくても大丈夫
    ++current_index;
  }

  for (Square k : SQ) {
    for (Eval::BonaPiece p0 = Eval::BONA_PIECE_ZERO; p0 < Eval::fe_end; ++p0) {
      for (Eval::BonaPiece p1 = Eval::BONA_PIECE_ZERO; p1 < Eval::fe_end; ++p1) {
        for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
          clamp_and_store(current_index, kpp_index_to_raw_index(k, p0, p1, c), Eval::kpp[k][p0][p1][c], sum_w, w, last_update_indexes);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Eval::BonaPiece p = Eval::BONA_PIECE_ZERO; p < Eval::fe_end; ++p) {
        for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
          clamp_and_store(current_index, kkp_index_to_raw_index(k0, k1, p, c), Eval::kkp[k0][k1][p][c], sum_w, w, last_update_indexes);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Color c = COLOR_ZERO; c < COLOR_NB; ++c) {
        clamp_and_store(current_index, kk_index_to_raw_index(k0, k1, c), Eval::kk[k0][k1][c], sum_w, w, last_update_indexes);
      }
    }
  }

  Eval::save_eval();
}
