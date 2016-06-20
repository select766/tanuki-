#include "learner.h"

#include <array>
#include <ctime>
#include <fstream>

#include "kifu_reader.h"
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

  void save_eval(const std::string& folderPath);
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

  struct Weight
  {
    // 重み
    WeightType w;
    // Adam用変数
    WeightType m;
    WeightType v;
    WeightType adam_beta1_t = 1.0;
    WeightType adam_beta2_t = 1.0;
    // 平均化確率的勾配降下法用変数
    //WeightType sum_w;
    //int64_t last_update_index;

    template<typename T>
    void update(int64_t position_index, double dt, T& eval_weight);
    template<typename T>
    void finalize(int64_t position_index, T& eval_weight);
  };

  constexpr char* kFolderName = "kifu";
  constexpr char* kOutputFolderPathBase = "eval";
  constexpr int kFvScale = 32;
  constexpr WeightType kEps = 1e-8;
  constexpr WeightType kAdamBeta1 = 0.9;
  constexpr WeightType kAdamBeta2 = 0.999;
  constexpr WeightType kLearningRate = 1.0;
  constexpr int kMaxGamePlay = 256;
  constexpr int64_t kWriteEvalPerPosition = 1000000000; // 10億
  constexpr int64_t kMaxPositionsForErrorMeasurement = 10000000; // 1千万

  std::unique_ptr<Learner::KifuReader> kifu_reader;

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
    m = kAdamBeta1 * m + (1.0 - kAdamBeta1) * dt;
    v = kAdamBeta2 * v + (1.0 - kAdamBeta2) * dt * dt;
    // 高速化のためpow(ADAM_BETA1, t)の値を保持しておく
    adam_beta1_t *= kAdamBeta1;
    adam_beta2_t *= kAdamBeta2;
    WeightType mm = m / (1.0 - adam_beta1_t);
    WeightType vv = v / (1.0 - adam_beta2_t);
    WeightType delta = kLearningRate * mm / (sqrt(vv) + kEps);
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

#if 1
    // evaluate()の値を直接使う場合
    // evaluate()は手番から見た評価値を返すので
    // 符号の反転はしなくて良い
    value = Eval::evaluate(pos);
#elif 0
    // 1手読みを行う場合
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
#elif 0
    // 1手読みを行う場合
    pos.check_info_update();

    // qsearch()相当の探索を行う
    // 本当はqsearch()を呼びたいが、呼んだらクラッシュしたので…。
    thread.rootMoves.clear();
    // 王手が掛かっているかどうかに応じてrootMovesの種類を変える
    if (pos.in_check()) {
      for (auto m : MoveList<EVASIONS>(pos)) {
        if (pos.legal(m)) {
          thread.rootMoves.push_back(Search::RootMove(m));
        }
      }
    }
    else {
      for (auto m : MoveList<CAPTURES_PRO_PLUS>(pos)) {
        if (pos.legal(m)) {
          thread.rootMoves.push_back(Search::RootMove(m));
        }
      }
      for (auto m : MoveList<QUIET_CHECKS>(pos)) {
        if (pos.legal(m)) {
          thread.rootMoves.push_back(Search::RootMove(m));
        }
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
#else
    static_assert(false, "Choose a method to search shallowly.");
#endif

    return true;
  }

  double sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
  }

  double winning_percentage(Value value) {
    return sigmoid(static_cast<int>(value) / 600.0);
  }

  double dsigmoid(double x) {
    return sigmoid(x) * (1.0 - sigmoid(x));
  }

  std::string GetDateTimeString() {
    time_t time = std::time(nullptr);
    struct tm *tm = std::localtime(&time);
    char buffer[1024];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H-%M-%S", tm);
    return buffer;
  }
}

void Learner::learn()
{
  ASSERT_LV3(
    kk_index_to_raw_index(SQ_NB, SQ_ZERO, WEIGHT_KIND_NB) ==
    static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * WEIGHT_KIND_NB);

  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  kifu_reader = std::make_unique<Learner::KifuReader>(kFolderName);

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
  limits.max_game_ply = kMaxGamePlay;
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
      while (kifu_reader->Read(pos, record_value)) {
        int64_t position_index = global_position_index++;

        Value value;
        Color rootColor;
        pos.set_this_thread(&thread);
        if (!search_shallowly(pos, value, rootColor)) {
          continue;
        }

#if 1
        // 深い探索の評価値と浅い探索の評価値の二乗誤差を最小にする
        WeightType delta = static_cast<WeightType>((record_value - value) * kFvScale);
#elif 0
        // 深い深さの評価値から求めた勝率と浅い探索の評価値の二乗誤差を最小にする
        WeightType delta = (sigmoid(static_cast<int>(value) / 600.0) - sigmoid(static_cast<int>(record_value) / 600.0))
          * dsigmoid(static_cast<int>(value) / 600.0);
#else
        static_assert(false, "Choose a loss function.");
#endif
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

          fprintf(stderr,
            "position_index=%I64d record=%5d (%.2f)\n"
            "    before=%5d (%.2f)  diff=%5d\n"
            "     after=%5d (%.2f) delta=%5d error=%s\n",
            position_index,
            static_cast<int>(record_value), winning_percentage(record_value),
            static_cast<int>(value), winning_percentage(value),
            static_cast<int>(record_value - value),
            static_cast<int>(value_after), winning_percentage(value_after),
            static_cast<int>(value_after - value),
            abs(record_value - value) > abs(record_value - value_after) ? "↓" :
            abs(record_value - value) == abs(record_value - value_after) ? "＝" : "↑");
        }

        if (position_index > 0 && position_index % kWriteEvalPerPosition == 0) {
          char buffer[1024];
          sprintf(buffer, "%s/%I64d", kOutputFolderPathBase, position_index);
          fprintf(stderr, "Writing eval files: %s\n", buffer);
          Eval::save_eval(buffer);
        }

        // 局面は元に戻さなくても問題ない
      }
    };

    threads.emplace_back(procedure);
  }
  for (auto& thread : threads) {
    thread.join();
  }

  fprintf(stderr, "Finalizing weights\n");
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

  std::string folderPath = std::string(kOutputFolderPathBase) + "/" + GetDateTimeString();
  fprintf(stderr, "Writing eval files: %s\n", folderPath.c_str());
  Eval::save_eval(folderPath);
}

void Learner::error_measurement()
{
  ASSERT_LV3(
    kk_index_to_raw_index(SQ_NB, SQ_ZERO, WEIGHT_KIND_ZERO) ==
    static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * WEIGHT_KIND_NB);

  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  kifu_reader = std::make_unique<KifuReader>(kFolderName);

  Eval::load_eval();

  Search::LimitsType limits;
  limits.max_game_ply = kMaxGamePlay;
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
      while (global_position_index < kMaxPositionsForErrorMeasurement &&
        kifu_reader->Read(pos, record_value)) {
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
