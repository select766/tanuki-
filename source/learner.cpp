#include "learner.h"

#include <array>
#include <ctime>
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
  using WeightType = float;

  enum WeightKind {
    WEIGHT_KIND_COLOR,
    WEIGHT_KIND_TURN,
    WEIGHT_KIND_ZERO = 0,
    WEIGHT_KIND_NB = 2,
  };
  ENABLE_OPERATORS_ON(WeightKind);

  struct PositionAndValue
  {
    std::string sfen;
    Value value;
  };

  struct Weight
  {
    // 重み
    WeightType w;
    // Adam用変数
    WeightType m;
    WeightType v;
    int t;
    // 平均化確率的勾配降下法用変数
    //WeightType sum_w;
    //int64_t last_update_index;

    template<typename T>
    void update(int64_t position_index, double dt, T& eval_weight);
    template<typename T>
    void finalize(int64_t position_index, T& eval_weight);
  };

  constexpr char* KIFU_FILE_NAME = "kifu/kifu.2016-06-01.1000000.csv";
  constexpr Value CLOSE_OUT_VALUE_THRESHOLD = Value(2000);
  constexpr int POSITION_BATCH_SIZE = 1000000;
  constexpr int FV_SCALE = 32;
  constexpr WeightType EPS = 1e-8;
  constexpr WeightType ADAM_BETA1 = 0.9;
  constexpr WeightType ADAM_BETA2 = 0.999;
  constexpr WeightType LEARNING_RATE = 1.0;
  constexpr int MAX_GAME_PLAY = 256;
  constexpr int64_t MAX_POSITIONS_FOR_ERROR_MEASUREMENT = 1000000;
  constexpr int MAX_KIFU_FILE_LOOP = 1;

  std::ifstream kifu_file_stream;
  std::vector<PositionAndValue> position_and_values;
  int position_and_value_index = 0;
  int kifu_file_loop = 0;

  bool open_kifu()
  {
    kifu_file_stream.open(KIFU_FILE_NAME);
    if (!kifu_file_stream.is_open()) {
      sync_cout << "info string Failed to open the kifu file." << sync_endl;
      return false;
    }
    return true;
  }

  bool try_fill_buffer() {
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
    return true;
  }

  bool read_position_and_value(Position& pos, Value& value)
  {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock_guard(mutex);

    if (kifu_file_loop > MAX_KIFU_FILE_LOOP) {
      return false;
    }

    if (position_and_value_index >= static_cast<int>(position_and_values.size())) {
      // 局面バッファを最後まで使い切った場合は
      // 棋譜ファイルから局面をバッファに読み込む
      if (!try_fill_buffer()) {
        // 棋譜ファイルから局面をバッファに読み込めなかった場合は
        // 棋譜ファイルを開き直す
        if (kifu_file_loop++ >= MAX_KIFU_FILE_LOOP) {
          // ファイルの読み込み回数の上限に達していたら終了する
          return false;
        }

        if (!open_kifu()) {
          // そもそもファイルを開くことができない場合は終了する
          return false;
        }

        // 棋譜ファイルから局面バッファに読み込む
        if (!try_fill_buffer()) {
          // 棋譜ファイルから局面バッファに読み込めなかった場合は終了する
          return false;
        }
      }
    }

    pos.set(position_and_values[position_and_value_index].sfen);
    value = position_and_values[position_and_value_index].value;
    ++position_and_value_index;
    return true;
  }

  int kpp_index_to_raw_index(Square k, Eval::BonaPiece p0, Eval::BonaPiece p1, WeightKind weight_kind) {
    return static_cast<int>(static_cast<int>(static_cast<int>(k) * Eval::fe_end + p0) * Eval::fe_end + p1) * WEIGHT_KIND_NB + weight_kind;
  }

  int kkp_index_to_raw_index(Square k0, Square k1, Eval::BonaPiece p, WeightKind weight_kind) {
    return kpp_index_to_raw_index(SQ_NB, Eval::BONA_PIECE_ZERO, Eval::BONA_PIECE_ZERO, WEIGHT_KIND_NB) +
      static_cast<int>(static_cast<int>(static_cast<int>(k0) * SQ_NB + k1) * Eval::fe_end + p) * COLOR_NB + weight_kind;
  }

  int kk_index_to_raw_index(Square k0, Square k1, WeightKind weight_kind) {
    return kkp_index_to_raw_index(SQ_NB, SQ_ZERO, Eval::BONA_PIECE_ZERO, WEIGHT_KIND_NB) +
      (static_cast<int>(static_cast<int>(k0) * SQ_NB + k1) * COLOR_NB) + weight_kind;
  }

  template<typename T>
  void Weight::update(int64_t position_index, double dt, T& eval_weight)
  {
    //WeightType previous_w = w;

    // Adam
    m = ADAM_BETA1 * m + (1.0 - ADAM_BETA1) * dt;
    v = ADAM_BETA2 * v + (1.0 - ADAM_BETA2) * dt * dt;
    ++t;
    WeightType mm = m / (1.0 - pow(ADAM_BETA1, t));
    WeightType vv = v / (1.0 - pow(ADAM_BETA2, t));
    WeightType delta = LEARNING_RATE * mm / (sqrt(vv) + EPS);
    w += delta;

    // 平均化確率的勾配降下法
    //sum_w += previous_w * (current_index - last_update_index);
    //last_update_index = current_index;

    // 重みテーブルに書き戻す
    eval_weight = static_cast<T>(std::round(w));
  }

  template<typename T>
  void Weight::finalize(int64_t position_index, T& eval_weight)
  {
    //sum_w += w * (position_index - last_update_index);
    //int64_t value = static_cast<int64_t>(std::round(sum_w / current_index));
    int64_t value = static_cast<int64_t>(std::round(w));
    value = std::max<int64_t>(std::numeric_limits<T>::min(), value);
    value = std::min<int64_t>(std::numeric_limits<T>::max(), value);
    eval_weight = static_cast<T>(value);
  }

  // 浅く探索する
  // pos 探索対象の局面
  // value 浅い探索の評価値
  // rootColor 探索対象の局面の手番
  // return 浅い探索の評価値とPVの末端ノードの評価値が一致していればtrue
  //        そうでない場合はfalse
  bool search_shallowly(Position& pos, Value& value, Color& root_color) {
    Thread& thread = *pos.this_thread();
    root_color = pos.side_to_move();

    pos.check_info_update();

    // 浅い探索を行う
    // 本当はqsearch()で行いたいが、直接呼んだらクラッシュしたので…。
    thread.rootMoves.clear();
    for (auto m : MoveList<LEGAL>(pos)) {
      if (pos.legal(m)) {
        thread.rootMoves.push_back(Search::RootMove(m));
      }
    }

    if (thread.rootMoves.empty()) {
      // 現在の局面が静止している状態なのだと思う
      // 現在の局面の評価値をそのまま使う
      value = Eval::evaluate(pos);
    }
    else {
      // 実際に探索する
      thread.maxPly = 0;
      thread.rootDepth = 0;

      // 探索スレッドを使わずに直接Thread::search()を呼び出して探索する
      // こちらのほうが探索スレッドを起こさないので速いはず
      thread.search();
      value = thread.rootMoves[0].score;

      // 静止した局面まで進める
      StateInfo stateInfo[MAX_PLY];
      int play = 0;
      // Eval::evaluate()を使うと差分計算のおかげで少し速くなるはず
      // 全計算はPosition::set()の中で行われているので差分計算ができる
      Value value_pv = Eval::evaluate(pos);
      for (auto m : thread.rootMoves[0].pv) {
        pos.do_move(m, stateInfo[play++]);
        value_pv = Eval::evaluate(pos);
      }

      // Eval::evaluate()は常に手番から見た評価値を返すので
      // 探索開始局面と手番が違う場合は符号を反転する
      if (root_color != pos.side_to_move()) {
        value_pv = -value_pv;
      }

      // 浅い探索の評価値とPVの末端ノードの評価値が食い違う場合は
      // 処理に含めないようfalseを返す
      // 全体の9%程度しかないので無視しても大丈夫だと思いたい…。
      if (value != value_pv) {
        return false;
      }
    }

    return true;
  }
}

void Learner::learn()
{
  ASSERT_LV3(
    kk_index_to_raw_index(SQ_NB, SQ_ZERO, WEIGHT_KIND_NB) ==
    static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * WEIGHT_KIND_NB);

  std::srand(std::time(nullptr));

  Eval::load_eval();

  int vector_length = kk_index_to_raw_index(SQ_NB, SQ_ZERO, WEIGHT_KIND_ZERO);

  std::vector<Weight> weights(vector_length);
  memset(&weights[0], 0, sizeof(weights[0]) * weights.size());

  for (Square k : SQ) {
    for (Eval::BonaPiece p0 = Eval::BONA_PIECE_ZERO; p0 < Eval::fe_end; ++p0) {
      for (Eval::BonaPiece p1 = Eval::BONA_PIECE_ZERO; p1 < Eval::fe_end; ++p1) {
        for (WeightKind weight_kind = WEIGHT_KIND_ZERO; weight_kind < WEIGHT_KIND_NB; ++weight_kind) {
          weights[kpp_index_to_raw_index(k, p0, p1, weight_kind)].w =
            static_cast<WeightType>(Eval::kpp[k][p0][p1][weight_kind]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Eval::BonaPiece p = Eval::BONA_PIECE_ZERO; p < Eval::fe_end; ++p) {
        for (WeightKind weight_kind = WEIGHT_KIND_ZERO; weight_kind < WEIGHT_KIND_NB; ++weight_kind) {
          weights[kkp_index_to_raw_index(k0, k1, p, weight_kind)].w =
            static_cast<WeightType>(Eval::kkp[k0][k1][p][weight_kind]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (WeightKind weight_kind = WEIGHT_KIND_ZERO; weight_kind < WEIGHT_KIND_NB; ++weight_kind) {
        weights[kk_index_to_raw_index(k0, k1, weight_kind)].w =
          static_cast<WeightType>(Eval::kk[k0][k1][weight_kind]);
      }
    }
  }

  Search::LimitsType limits;
  limits.max_game_ply = MAX_GAME_PLAY;
  limits.depth = 1;
  limits.silent = true;
  Search::Limits = limits;

  std::atomic_int64_t global_position_index = 0;
  std::vector<std::thread> threads;
  while (threads.size() < Threads.size()) {
    int thread_index = static_cast<int>(threads.size());
    auto procedure = [&global_position_index, &threads, &weights, thread_index] {
      Thread& thread = *Threads[thread_index];

      Position& pos = thread.rootPos;
      Value record_value;
      while (read_position_and_value(pos, record_value)) {
        int64_t position_index = global_position_index++;

        Value value;
        Color rootColor;
        pos.set_this_thread(&thread);
        if (!search_shallowly(pos, value, rootColor)) {
          continue;
        }

        // 深い深さの探索による評価値との差分を求める
        WeightType delta = static_cast<WeightType>((record_value - value) * FV_SCALE);
        // 先手から見た評価値の差分。sum.p[?][0]に足したり引いたりする。
        WeightType delta_color = (rootColor == BLACK ? delta : -delta);
        // 手番から見た評価値の差分。sum.p[?][1]に足したり引いたりする。
        WeightType delta_turn = (rootColor == pos.side_to_move() ? delta : -delta);

        // 値を更新する
        Square sq_bk = pos.king_square(BLACK);
        Square sq_wk = pos.king_square(WHITE);
        const auto& list0 = pos.eval_list()->piece_list_fb();
        const auto& list1 = pos.eval_list()->piece_list_fw();

        // KK
        weights[kk_index_to_raw_index(sq_bk, sq_wk, WEIGHT_KIND_COLOR)].update(position_index, delta_color, Eval::kk[sq_bk][sq_wk][WEIGHT_KIND_COLOR]);
        weights[kk_index_to_raw_index(sq_bk, sq_wk, WEIGHT_KIND_TURN)].update(position_index, delta_turn, Eval::kk[sq_bk][sq_wk][WEIGHT_KIND_TURN]);

        for (int i = 0; i < PIECE_NO_KING; ++i) {
          Eval::BonaPiece k0 = list0[i];
          Eval::BonaPiece k1 = list1[i];
          for (int j = 0; j < i; ++j) {
            Eval::BonaPiece l0 = list0[j];
            Eval::BonaPiece l1 = list1[j];

            // KPP
            weights[kpp_index_to_raw_index(sq_bk, k0, l0, WEIGHT_KIND_COLOR)].update(position_index, delta_color, Eval::kpp[sq_bk][k0][l0][WEIGHT_KIND_COLOR]);
            weights[kpp_index_to_raw_index(sq_bk, l0, k0, WEIGHT_KIND_COLOR)] = weights[kpp_index_to_raw_index(sq_bk, k0, l0, WEIGHT_KIND_COLOR)];
            Eval::kpp[sq_bk][l0][k0][WEIGHT_KIND_COLOR] = Eval::kpp[sq_bk][k0][l0][WEIGHT_KIND_COLOR];

            weights[kpp_index_to_raw_index(sq_bk, k0, l0, WEIGHT_KIND_TURN)].update(position_index, delta_turn, Eval::kpp[sq_bk][k0][l0][WEIGHT_KIND_TURN]);
            weights[kpp_index_to_raw_index(sq_bk, l0, k0, WEIGHT_KIND_TURN)] = weights[kpp_index_to_raw_index(sq_bk, k0, l0, WEIGHT_KIND_TURN)];
            Eval::kpp[sq_bk][l0][k0][WEIGHT_KIND_TURN] = Eval::kpp[sq_bk][k0][l0][WEIGHT_KIND_TURN];

            // KPP
            weights[kpp_index_to_raw_index(Inv(sq_wk), k1, l1, WEIGHT_KIND_COLOR)].update(position_index, -delta_color, Eval::kpp[Inv(sq_wk)][k1][l1][WEIGHT_KIND_COLOR]);
            weights[kpp_index_to_raw_index(Inv(sq_wk), l1, k1, WEIGHT_KIND_COLOR)] = weights[kpp_index_to_raw_index(Inv(sq_wk), k1, l1, WEIGHT_KIND_COLOR)];
            Eval::kpp[Inv(sq_wk)][l1][k1][WEIGHT_KIND_COLOR] = Eval::kpp[Inv(sq_wk)][k1][l1][WEIGHT_KIND_COLOR];

            weights[kpp_index_to_raw_index(Inv(sq_wk), k1, l1, WEIGHT_KIND_TURN)].update(position_index, delta_turn, Eval::kpp[Inv(sq_wk)][k1][l1][WEIGHT_KIND_TURN]);
            weights[kpp_index_to_raw_index(Inv(sq_wk), l1, k1, WEIGHT_KIND_TURN)] = weights[kpp_index_to_raw_index(Inv(sq_wk), k1, l1, WEIGHT_KIND_TURN)];
            Eval::kpp[Inv(sq_wk)][l1][k1][WEIGHT_KIND_TURN] = Eval::kpp[Inv(sq_wk)][k1][l1][WEIGHT_KIND_TURN];
          }

          // KKP
          weights[kkp_index_to_raw_index(sq_bk, sq_wk, k0, WEIGHT_KIND_COLOR)].update(position_index, delta_color, Eval::kkp[sq_bk][sq_wk][k0][WEIGHT_KIND_COLOR]);
          weights[kkp_index_to_raw_index(sq_bk, sq_wk, k0, WEIGHT_KIND_TURN)].update(position_index, delta_turn, Eval::kkp[sq_bk][sq_wk][k0][WEIGHT_KIND_TURN]);
        }

        if (position_index % 10000 == 0) {
          Value value_after = Eval::compute_eval(pos);
          if (rootColor != pos.side_to_move()) {
            value_after = -value_after;
          }

          fprintf(stderr, "position_index=%I64d recorded=%5d before=%5d diff=%5d after=%5d delta=%5d target=%c turn=%s error=%c\n",
            position_index,
            static_cast<int>(record_value),
            static_cast<int>(value),
            static_cast<int>(record_value - value),
            static_cast<int>(value_after),
            static_cast<int>(value_after - value),
            delta > 0 ? '+' : '-',
            rootColor == BLACK ? "black" : "white",
            abs(record_value - value) > abs(record_value - value_after) ? '>' :
            abs(record_value - value) == abs(record_value - value_after) ? '=' : '<');
        }

        // 局面は元に戻さなくても問題ない
      }
    };

    threads.emplace_back(procedure);
  }
  for (auto& thread : threads) {
    thread.join();
  }

  for (Square k : SQ) {
    for (Eval::BonaPiece p0 = Eval::BONA_PIECE_ZERO; p0 < Eval::fe_end; ++p0) {
      for (Eval::BonaPiece p1 = Eval::BONA_PIECE_ZERO; p1 < Eval::fe_end; ++p1) {
        for (WeightKind weight_kind = WEIGHT_KIND_ZERO; weight_kind < WEIGHT_KIND_NB; ++weight_kind) {
          weights[kpp_index_to_raw_index(k, p0, p1, weight_kind)].finalize(global_position_index, Eval::kpp[k][p0][p1][weight_kind]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (Eval::BonaPiece p = Eval::BONA_PIECE_ZERO; p < Eval::fe_end; ++p) {
        for (WeightKind weight_kind = WEIGHT_KIND_ZERO; weight_kind < WEIGHT_KIND_NB; ++weight_kind) {
          weights[kkp_index_to_raw_index(k0, k1, p, weight_kind)].finalize(global_position_index, Eval::kkp[k0][k1][p][weight_kind]);
        }
      }
    }
  }
  for (Square k0 : SQ) {
    for (Square k1 : SQ) {
      for (WeightKind weight_kind = WEIGHT_KIND_ZERO; weight_kind < WEIGHT_KIND_NB; ++weight_kind) {
        weights[kk_index_to_raw_index(k0, k1, weight_kind)].finalize(global_position_index, Eval::kk[k0][k1][weight_kind]);
      }
    }
  }

  Eval::save_eval();
}

void Learner::error_measurement()
{
  ASSERT_LV3(
    kk_index_to_raw_index(SQ_NB, SQ_ZERO, WEIGHT_KIND_ZERO) ==
    static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * WEIGHT_KIND_NB);

  std::srand(std::time(nullptr));

  Eval::load_eval();

  if (!open_kifu()) {
    sync_cout << "info string Failed to open the kifu file." << sync_endl;
    return;
  }

  Search::LimitsType limits;
  limits.max_game_ply = MAX_GAME_PLAY;
  limits.depth = 1;
  limits.silent = true;
  Search::Limits = limits;

  std::atomic_int64_t global_position_index = 0;
  std::vector<std::thread> threads;
  double global_error = 0.0;
  double global_norm = 0.0;
  while (threads.size() < Threads.size()) {
    int thread_index = static_cast<int>(threads.size());
    auto procedure = [thread_index, &global_position_index, &threads, &global_error, &global_norm] {
      double error = 0.0;
      double norm = 0.0;
      Thread& thread = *Threads[thread_index];

      Position& pos = thread.rootPos;
      Value record_value;
      while (global_position_index < MAX_POSITIONS_FOR_ERROR_MEASUREMENT &&
        read_position_and_value(pos, record_value)) {
        int64_t position_index = global_position_index++;

        Value value;
        Color rootColor;
        pos.set_this_thread(&thread);
        if (!search_shallowly(pos, value, rootColor)) {
          continue;
        }

        double diff = record_value - value;
        error += diff * diff;
        norm += abs(value);

        if (position_index % 100000 == 0) {
          Value value_after = Eval::compute_eval(pos);
          if (rootColor != pos.side_to_move()) {
            value_after = -value_after;
          }

          fprintf(stderr, "index=%I64d\n", position_index);
        }
      }

      static std::mutex mutex;
      std::lock_guard<std::mutex> lock_guard(mutex);
      global_error += error;
      global_norm += abs(static_cast<int>(norm));
    };

    threads.emplace_back(procedure);
  }
  for (auto& thread : threads) {
    thread.join();
  }

  printf(
    "info string mse=%f norm=%f\n",
    sqrt(global_error / global_position_index),
    global_norm / global_position_index);
}
