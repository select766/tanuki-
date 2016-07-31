#include "experimental_learner.h"

#include <array>
#include <ctime>
#include <fstream>
#include <direct.h>
#include <omp.h>

#include "kifu_reader.h"
#include "position.h"
#include "search.h"
#include "thread.h"

// Gradient Noise
#define GRADIENT_NOISE

namespace Learner
{
  std::pair<Value, std::vector<Move> > search(Position& pos, Value alpha, Value beta, int depth);
  std::pair<Value, std::vector<Move> > qsearch(Position& pos, Value alpha, Value beta);
}

namespace Eval
{
  typedef std::array<int16_t, 2> ValueKpp;
  typedef std::array<int32_t, 2> ValueKkp;
  typedef std::array<int32_t, 2> ValueKk;
  // FV_SCALEで割る前の値が入る
  extern ALIGNED(32) ValueKk kk[SQ_NB][SQ_NB];
  extern ALIGNED(32) ValueKpp kpp[SQ_NB][fe_end][fe_end];
  extern ALIGNED(32) ValueKkp kkp[SQ_NB][SQ_NB][fe_end];
  extern const int FV_SCALE = 32;

  void save_eval();
}

namespace
{
  using WeightType = double;

  enum WeightKind {
    WEIGHT_KIND_COLOR,
    WEIGHT_KIND_TURN,
    WEIGHT_KIND_ZERO = 0,
    WEIGHT_KIND_NB = 2,
  };
  ENABLE_FULL_OPERATORS_ON(WeightKind);

  struct Weight
  {
    // 勾配の和
    WeightType sum_gradient;
    // 重み
    WeightType w;
    // Adam用変数
    WeightType m;
    WeightType v;
    // 平均化確率的勾配降下法用変数
    WeightType sum_w;

    void AddGradient(double gradient);
    template<typename T>
    void UpdateWeight(
        double adam_beta1_t, double adam_beta2_t, double learning_rate, T& eval_weight);
    template<typename T>
    void Finalize(int num_mini_batches, T& eval_weight);
  };


  constexpr int kFvScale = 32;
  constexpr WeightType kEps = 1e-8;
  constexpr WeightType kAdamBeta1 = 0.9;
  constexpr WeightType kAdamBeta2 = 0.999;
  constexpr WeightType kInitialLearningRate = 0.5;
  constexpr WeightType kLearningRateDecayRate = 1.0;
  constexpr int64_t kNumPositionsToDecayLearningRate = 5'0000'0000LL;
  constexpr int kMaxGamePlay = 256;
  constexpr int64_t kMaxPositionsForErrorMeasurement = 1000'0000LL;
  constexpr int64_t kMaxPositionsForLearning = 100'0000'0000LL;
  constexpr int64_t kMiniBatchSize = 10'0000LL;
#ifdef GRADIENT_NOISE
  constexpr double kGradientNoiseEta = 0.01;
  constexpr double kGradientNoiseTau = 0.55;
#endif

  int KppIndexToRawIndex(Square k, Eval::BonaPiece p0, Eval::BonaPiece p1, WeightKind weight_kind) {
    return static_cast<int>(static_cast<int>(static_cast<int>(k) * Eval::fe_end + p0) * Eval::fe_end + p1) * WEIGHT_KIND_NB + weight_kind;
  }

  int KkpIndexToRawIndex(Square k0, Square k1, Eval::BonaPiece p, WeightKind weight_kind) {
    return KppIndexToRawIndex(SQ_NB, Eval::BONA_PIECE_ZERO, Eval::BONA_PIECE_ZERO, WEIGHT_KIND_ZERO) +
      static_cast<int>(static_cast<int>(static_cast<int>(k0) * SQ_NB + k1) * Eval::fe_end + p) * COLOR_NB + weight_kind;
  }

  int KkIndexToRawIndex(Square k0, Square k1, WeightKind weight_kind) {
    return KkpIndexToRawIndex(SQ_NB, SQ_ZERO, Eval::BONA_PIECE_ZERO, WEIGHT_KIND_ZERO) +
      (static_cast<int>(static_cast<int>(k0) * SQ_NB + k1) * COLOR_NB) + weight_kind;
  }

  bool IsKppIndex(int index) {
    return 0 <= index &&
      index < KppIndexToRawIndex(SQ_NB, Eval::BONA_PIECE_ZERO, Eval::BONA_PIECE_ZERO, WEIGHT_KIND_ZERO);
  }

  bool IsKkpIndex(int index) {
    return 0 <= index &&
      !IsKppIndex(index) &&
      index < KkpIndexToRawIndex(SQ_NB, SQ_ZERO, Eval::BONA_PIECE_ZERO, WEIGHT_KIND_ZERO);
  }

  bool IsKkIndex(int index) {
    return 0 <= index &&
      !IsKppIndex(index) &&
      !IsKkpIndex(index) &&
      index < KkIndexToRawIndex(SQ_NB, SQ_ZERO, WEIGHT_KIND_ZERO);
  }

  void RawIndexToKppIndex(int dimension_index, Square& k, Eval::BonaPiece& p0, Eval::BonaPiece& p1, WeightKind& weight_kind) {
    int original_dimension_index = dimension_index;
    ASSERT_LV3(IsKppIndex(dimension_index));
    weight_kind = static_cast<WeightKind>(dimension_index % WEIGHT_KIND_NB);
    dimension_index /= WEIGHT_KIND_NB;
    p1 = static_cast<Eval::BonaPiece>(dimension_index % Eval::fe_end);
    dimension_index /= Eval::fe_end;
    p0 = static_cast<Eval::BonaPiece>(dimension_index % Eval::fe_end);
    dimension_index /= Eval::fe_end;
    k = static_cast<Square>(dimension_index);
    ASSERT_LV3(k < SQ_NB);
    ASSERT_LV3(KppIndexToRawIndex(k, p0, p1, weight_kind) == original_dimension_index);
  }

  void RawIndexToKkpIndex(int dimension_index, Square& k0, Square& k1, Eval::BonaPiece& p, WeightKind& weight_kind) {
    int original_dimension_index = dimension_index;
    ASSERT_LV3(IsKkpIndex(dimension_index));
    dimension_index -= KppIndexToRawIndex(SQ_NB, Eval::BONA_PIECE_ZERO, Eval::BONA_PIECE_ZERO, WEIGHT_KIND_ZERO);
    weight_kind = static_cast<WeightKind>(dimension_index % WEIGHT_KIND_NB);
    dimension_index /= WEIGHT_KIND_NB;
    p = static_cast<Eval::BonaPiece>(dimension_index % Eval::fe_end);
    dimension_index /= Eval::fe_end;
    k1 = static_cast<Square>(dimension_index % SQ_NB);
    dimension_index /= SQ_NB;
    k0 = static_cast<Square>(dimension_index);
    ASSERT_LV3(k0 < SQ_NB);
    ASSERT_LV3(KkpIndexToRawIndex(k0, k1, p, weight_kind) == original_dimension_index);
  }

  void RawIndexToKkIndex(int dimension_index, Square& k0, Square& k1, WeightKind& weight_kind) {
    int original_dimension_index = dimension_index;
    ASSERT_LV3(IsKkIndex(dimension_index));
    dimension_index -= KkpIndexToRawIndex(SQ_NB, SQ_ZERO, Eval::BONA_PIECE_ZERO, WEIGHT_KIND_ZERO);
    weight_kind = static_cast<WeightKind>(dimension_index % WEIGHT_KIND_NB);
    dimension_index /= WEIGHT_KIND_NB;
    k1 = static_cast<Square>(dimension_index % SQ_NB);
    dimension_index /= SQ_NB;
    k0 = static_cast<Square>(dimension_index);
    ASSERT_LV3(k0 < SQ_NB);
    ASSERT_LV3(KkIndexToRawIndex(k0, k1, weight_kind) == original_dimension_index);
  }

  void Weight::AddGradient(double gradient) {
    sum_gradient += gradient;
  }

  template<typename T>
  void Weight::UpdateWeight(double adam_beta1_t, double adam_beta2_t, double learning_rate, T& eval_weight) {
    // Adam
    m = kAdamBeta1 * m + (1.0 - kAdamBeta1) * sum_gradient;
    v = kAdamBeta2 * v + (1.0 - kAdamBeta2) * sum_gradient * sum_gradient;
    // 高速化のためpow(ADAM_BETA1, t)の値を保持しておく
    WeightType mm = m / (1.0 - adam_beta1_t);
    WeightType vv = v / (1.0 - adam_beta2_t);
    WeightType delta = learning_rate * mm / (std::sqrt(vv) + kEps);
    w -= delta;

    // 平均化確率的勾配降下法
    sum_w += w;

    // 重みテーブルに書き戻す
    eval_weight = static_cast<T>(std::round(w));

    sum_gradient = 0.0;
  }

  template<typename T>
  void Weight::Finalize(int num_mini_batches, T& eval_weight)
  {
    int64_t value = static_cast<int64_t>(std::round(sum_w / num_mini_batches));
    //int64_t value = static_cast<int64_t>(std::round(w));
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

#if 0
    // evaluate()の値を直接使う場合
    // evaluate()は手番から見た評価値を返すので
    // 符号の反転はしなくて良い
    value = Eval::evaluate(pos);

#elif 1
    // 0手読み+静止探索を行う場合
    auto valueAndPv = Learner::qsearch(pos, -VALUE_INFINITE, VALUE_INFINITE);

    // Eval::evaluate()を使うと差分計算のおかげで少し速くなるはず
    // 全計算はPosition::set()の中で行われているので差分計算ができる
    value = Eval::evaluate(pos);
    StateInfo stateInfo[MAX_PLY];
    const std::vector<Move>& pv = valueAndPv.second;
    for (int play = 0; play < static_cast<int>(pv.size()) &&  pv[play] != MOVE_NONE; ++play) {
      pos.do_move(pv[play], stateInfo[play]);
      value = Eval::evaluate(pos);
    }

    // Eval::evaluate()は常に手番から見た評価値を返すので
    // 探索開始局面と手番が違う場合は符号を反転する
    if (root_color != pos.side_to_move()) {
      value = -value;
    }

#elif 0
    // 1手読み+静止探索を行う場合
    auto valueAndPv = Learner::search(pos, -VALUE_INFINITE, VALUE_INFINITE, 1);
    value = valueAndPv.first;

    // 静止した局面まで進める
    StateInfo stateInfo[MAX_PLY];
    const std::vector<Move>& pv = valueAndPv.second;
    for (int play = 0; play < static_cast<int>(pv.size()); ++play) {
      pos.do_move(pv[play], stateInfo[play]);
    }

    // TODO(nodchip): extract_pv_from_tt()を実装して使う

    //int play = 0;
    //// Eval::evaluate()を使うと差分計算のおかげで少し速くなるはず
    //// 全計算はPosition::set()の中で行われているので差分計算ができる
    //Value value_pv = Eval::evaluate(pos);
    //for (auto m : thread.rootMoves[0].pv) {
    //  pos.do_move(m, stateInfo[play++]);
    //  value_pv = Eval::evaluate(pos);
    //}

    //// Eval::evaluate()は常に手番から見た評価値を返すので
    //// 探索開始局面と手番が違う場合は符号を反転する
    //if (root_color != pos.side_to_move()) {
    //  value_pv = -value_pv;
    //}

    //// 浅い探索の評価値とPVの末端ノードの評価値が食い違う場合は
    //// 処理に含めないようfalseを返す
    //// 全体の9%程度しかないので無視しても大丈夫だと思いたい…。
    //if (value != value_pv) {
    //  return false;
    //}

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

  void save_eval(const std::string& output_folder_path_base, int64_t position_index) {
    _mkdir(output_folder_path_base.c_str());

    char buffer[1024];
    std::sprintf(buffer, "%s/%I64d", output_folder_path_base.c_str(), position_index);
    std::printf("Writing eval files: %s\n", buffer);
    std::fflush(stdout);
    Options["EvalDir"] = buffer;
    _mkdir(buffer);
    Eval::save_eval();
  }

  // 損失関数
  WeightType CalculateGradient(Value record_value, Value value) {
#if 0
    // 評価値の差の二乗和
    return static_cast<WeightType>((value - record_value) * kFvScale);
#elif 0
    // 評価値から推定した勝率の差の二乗和
    double p = winning_percentage(record_value);
    double q = winning_percentage(value);
    return (q - p) * dsigmoid(static_cast<int>(value) / 600.0);
#elif 1
    // 評価値から推定した勝率の分布の交差エントロピー
    double p = winning_percentage(record_value);
    double q = winning_percentage(value);
    return q - p;
#else
    static_assert(false, "Select a loss function.");
#endif
  }
}

void Learner::learn(std::istringstream& iss)
{
  omp_set_num_threads((int)Options["Threads"]);

  std::string output_folder_path_base = "learner_output/" + GetDateTimeString();
  std::string token;
  while (iss >> token) {
    if (token == "output_folder_path_base") {
      iss >> output_folder_path_base;
    }
  }

  ASSERT_LV3(
    KkIndexToRawIndex(SQ_NB, SQ_ZERO, WEIGHT_KIND_ZERO) ==
    static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * WEIGHT_KIND_NB);

  std::vector<int64_t> write_eval_per_positions = {
      std::numeric_limits<int64_t>::max(),
      20'0000LL,
      40'0000LL,
      60'0000LL,
      80'0000LL,
      100'0000LL,
      200'0000LL,
      400'0000LL,
      600'0000LL,
      800'0000LL,
      1000'0000LL,
      2000'0000LL,
      4000'0000LL,
      6000'0000LL,
      8000'0000LL,
      1'0000'0000LL,
      2'0000'0000LL,
      4'0000'0000LL,
      6'0000'0000LL,
      8'0000'0000LL,
      10'0000'0000LL,
      20'0000'0000LL,
      30'0000'0000LL,
      40'0000'0000LL,
      50'0000'0000LL,
      60'0000'0000LL,
      70'0000'0000LL,
      80'0000'0000LL,
      90'0000'0000LL,
      100'0000'0000LL,
  };
  std::sort(write_eval_per_positions.begin(), write_eval_per_positions.end());
  int write_eval_per_positions_index = 0;

  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  std::unique_ptr<Learner::KifuReader> kifu_reader = std::make_unique<Learner::KifuReader>((std::string)Options["KifuDir"], true);

  Eval::load_eval();

  int vector_length = KkIndexToRawIndex(SQ_NB, SQ_ZERO, WEIGHT_KIND_ZERO);

  std::vector<Weight> weights(vector_length);
  memset(&weights[0], 0, sizeof(weights[0]) * weights.size());

  // 評価関数テーブルから重みベクトルへ重みをコピーする
  // 並列化を効かせたいのでdimension_indexで回す
#pragma omp parallel for
  for (int dimension_index = 0; dimension_index < vector_length; ++dimension_index) {
    if (IsKppIndex(dimension_index)) {
      Square k;
      Eval::BonaPiece p0;
      Eval::BonaPiece p1;
      WeightKind weight_kind;
      RawIndexToKppIndex(dimension_index, k, p0, p1, weight_kind);
      weights[KppIndexToRawIndex(k, p0, p1, weight_kind)].w =
        static_cast<WeightType>(Eval::kpp[k][p0][p1][weight_kind]);

    }
    else if (IsKkpIndex(dimension_index)) {
      Square k0;
      Square k1;
      Eval::BonaPiece p;
      WeightKind weight_kind;
      RawIndexToKkpIndex(dimension_index, k0, k1, p, weight_kind);
      weights[KkpIndexToRawIndex(k0, k1, p, weight_kind)].w =
        static_cast<WeightType>(Eval::kkp[k0][k1][p][weight_kind]);

    }
    else if (IsKkIndex(dimension_index)) {
      Square k0;
      Square k1;
      WeightKind weight_kind;
      RawIndexToKkIndex(dimension_index, k0, k1, weight_kind);
      weights[KkIndexToRawIndex(k0, k1, weight_kind)].w =
        static_cast<WeightType>(Eval::kk[k0][k1][weight_kind]);

    }
    else {
      ASSERT_LV3(false);
    }
  }

  Search::LimitsType limits;
  limits.max_game_ply = kMaxGamePlay;
  limits.depth = 1;
  limits.silent = true;
  Search::Limits = limits;

  // 作成・破棄のコストが高いためループの外に宣言する
  std::vector<Record> records;

  // 全学習データに対してループを回す
  auto start = std::chrono::system_clock::now();
  int num_mini_batches = 0;
  double learning_rate = kInitialLearningRate;
  int64_t next_record_index_to_decay_learning_rate = kNumPositionsToDecayLearningRate;
  for (int64_t num_processed_positions = 0; num_processed_positions < kMaxPositionsForLearning;) {
    // 残り時間表示
    if (num_processed_positions) {
      auto current = std::chrono::system_clock::now();
      auto elapsed = current - start;
      double elapsed_sec = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
      int remaining_sec = static_cast<int>(elapsed_sec / num_processed_positions * (kMaxPositionsForLearning - num_processed_positions));
      int h = remaining_sec / 3600;
      int m = remaining_sec / 60 % 60;
      int s = remaining_sec % 60;

      time_t     current_time;
      struct tm  *local_time;

      time(&current_time);
      local_time = localtime(&current_time);
      std::printf("%I64d / %I64d (%04d-%02d-%02d %02d:%02d:%02d remaining %02d:%02d:%02d)\n",
        num_processed_positions, kMaxPositionsForLearning,
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
        local_time->tm_hour, local_time->tm_min, local_time->tm_sec, h, m, s);
      std::fflush(stdout);
    }

    int num_records = static_cast<int>(std::min(
      kMaxPositionsForLearning - num_processed_positions, kMiniBatchSize));
    if (!kifu_reader->Read(num_records, records)) {
      break;
    }
    ++num_mini_batches;

#pragma omp parallel
    {
        // ミニバッチ
        // num_records個の学習データの勾配の和を求めて重みを更新する
#pragma omp for
        for (int record_index = 0; record_index < num_records; ++record_index) {
            int thread_index = omp_get_thread_num();
            Thread& thread = *Threads[thread_index];
            Position& pos = thread.rootPos;
            pos.set_this_thread(&thread);

            pos.set(records[record_index].sfen);
            Value record_value = records[record_index].value;

            Value value;
            Color rootColor;
            pos.set_this_thread(&thread);
            if (!search_shallowly(pos, value, rootColor)) {
                continue;
            }

            WeightType delta = CalculateGradient(record_value, value);
            // 先手から見た評価値の差分。sum.p[?][0]に足したり引いたりする。
            WeightType delta_color = (rootColor == BLACK ? delta : -delta);
            // 手番から見た評価値の差分。sum.p[?][1]に足したり引いたりする。
            WeightType delta_turn = (rootColor == pos.side_to_move() ? delta : -delta);

            // 値を更新する
            Square sq_bk = pos.king_square(BLACK);
            Square sq_wk = pos.king_square(WHITE);
            const auto& list0 = pos.eval_list()->piece_list_fb();
            const auto& list1 = pos.eval_list()->piece_list_fw();

            // 勾配の値を加算する

            // KK
            weights[KkIndexToRawIndex(sq_bk, sq_wk, WEIGHT_KIND_COLOR)].AddGradient(delta_color);
            weights[KkIndexToRawIndex(sq_bk, sq_wk, WEIGHT_KIND_TURN)].AddGradient(delta_turn);

            for (int i = 0; i < PIECE_NO_KING; ++i) {
                Eval::BonaPiece k0 = list0[i];
                Eval::BonaPiece k1 = list1[i];
                for (int j = 0; j < i; ++j) {
                    Eval::BonaPiece l0 = list0[j];
                    Eval::BonaPiece l1 = list1[j];

                    // 常にp0 < p1となるようにアクセスする

                    // KPP
                    Eval::BonaPiece p0b = std::min(k0, l0);
                    Eval::BonaPiece p1b = std::max(k0, l0);
                    weights[KppIndexToRawIndex(sq_bk, p0b, p1b, WEIGHT_KIND_COLOR)].AddGradient(delta_color);
                    weights[KppIndexToRawIndex(sq_bk, p0b, p1b, WEIGHT_KIND_TURN)].AddGradient(delta_turn);

                    // KPP
                    Eval::BonaPiece p0w = std::min(k1, l1);
                    Eval::BonaPiece p1w = std::max(k1, l1);
                    weights[KppIndexToRawIndex(Inv(sq_wk), p0w, p1w, WEIGHT_KIND_COLOR)].AddGradient(-delta_color);
                    weights[KppIndexToRawIndex(Inv(sq_wk), p0w, p1w, WEIGHT_KIND_TURN)].AddGradient(delta_turn);
                }

                // KKP
                weights[KkpIndexToRawIndex(sq_bk, sq_wk, k0, WEIGHT_KIND_COLOR)].AddGradient(delta_color);
                weights[KkpIndexToRawIndex(sq_bk, sq_wk, k0, WEIGHT_KIND_TURN)].AddGradient(delta_turn);
            }

            // 局面は元に戻さなくても問題ない
        }

        // 重みを更新する
        double adam_beta1_t = std::pow(kAdamBeta1, num_mini_batches);
        double adam_beta2_t = std::pow(kAdamBeta2, num_mini_batches);

#ifdef GRADIENT_NOISE
        std::random_device random_device;
        std::mt19937_64 mt19937_64(random_device());
        std::normal_distribution<> normal_distribution(0.0, kGradientNoiseEta / pow(1.0 + num_mini_batches, kGradientNoiseTau));
#endif

        // 並列化を効かせたいのでdimension_indexで回す
#pragma omp for schedule(guided)
        for (int dimension_index = 0; dimension_index < vector_length; ++dimension_index) {
            if (IsKppIndex(dimension_index)) {
                Square k;
                Eval::BonaPiece p0;
                Eval::BonaPiece p1;
                WeightKind weight_kind;
                RawIndexToKppIndex(dimension_index, k, p0, p1, weight_kind);

                // 常にp0 < p1となるようにアクセスする
                if (p0 > p1) {
                    continue;
                }

                auto& weight = weights[KppIndexToRawIndex(k, p0, p1, weight_kind)];
#ifdef GRADIENT_NOISE
                weight.AddGradient(normal_distribution(mt19937_64));
#endif
                weight.UpdateWeight(adam_beta1_t, adam_beta2_t, learning_rate,
                  Eval::kpp[k][p0][p1][weight_kind]);
                Eval::kpp[k][p1][p0][weight_kind] = Eval::kpp[k][p0][p1][weight_kind];

            }
            else if (IsKkpIndex(dimension_index)) {
                Square k0;
                Square k1;
                Eval::BonaPiece p;
                WeightKind weight_kind;
                RawIndexToKkpIndex(dimension_index, k0, k1, p, weight_kind);
                auto& weight = weights[KkpIndexToRawIndex(k0, k1, p, weight_kind)];
#ifdef GRADIENT_NOISE
                weight.AddGradient(normal_distribution(mt19937_64));
#endif
                weight.UpdateWeight(adam_beta1_t, adam_beta2_t, learning_rate,
                  Eval::kkp[k0][k1][p][weight_kind]);

            }
            else if (IsKkIndex(dimension_index)) {
                Square k0;
                Square k1;
                WeightKind weight_kind;
                RawIndexToKkIndex(dimension_index, k0, k1, weight_kind);
                auto& weight = weights[KkIndexToRawIndex(k0, k1, weight_kind)];
#ifdef GRADIENT_NOISE
                weight.AddGradient(normal_distribution(mt19937_64));
#endif
                weight.UpdateWeight(adam_beta1_t, adam_beta2_t, learning_rate,
                  Eval::kk[k0][k1][weight_kind]);

            }
            else {
                ASSERT_LV3(false);
            }
        }
    }

    num_processed_positions += num_records;

    if (write_eval_per_positions[write_eval_per_positions_index] <= num_processed_positions) {
      save_eval(output_folder_path_base, num_processed_positions);
      ++write_eval_per_positions_index;
    }

    if (num_processed_positions >= next_record_index_to_decay_learning_rate) {
        learning_rate *= kLearningRateDecayRate;
        next_record_index_to_decay_learning_rate += kNumPositionsToDecayLearningRate;
        std::printf("Decayed the learning rate: learning_rate=%f\n", learning_rate);
        std::fflush(stdout);
    }
  }

  std::printf("Finalizing weights\n");
  std::fflush(stdout);

  // 平均化法をかけた後、評価関数テーブルに重みを書き出す
#pragma omp parallel for
  for (int dimension_index = 0; dimension_index < vector_length; ++dimension_index) {
    if (IsKppIndex(dimension_index)) {
      Square k;
      Eval::BonaPiece p0;
      Eval::BonaPiece p1;
      WeightKind weight_kind;
      RawIndexToKppIndex(dimension_index, k, p0, p1, weight_kind);

      // 常にp0 < p1となるようにアクセスする
      if (p0 > p1) {
        continue;
      }

      weights[KppIndexToRawIndex(k, p0, p1, weight_kind)].Finalize(
        num_mini_batches, Eval::kpp[k][p0][p1][weight_kind]);
      Eval::kpp[k][p1][p0][weight_kind] = Eval::kpp[k][p0][p1][weight_kind];

    }
    else if (IsKkpIndex(dimension_index)) {
      Square k0;
      Square k1;
      Eval::BonaPiece p;
      WeightKind weight_kind;
      RawIndexToKkpIndex(dimension_index, k0, k1, p, weight_kind);
      weights[KkpIndexToRawIndex(k0, k1, p, weight_kind)].Finalize(
        num_mini_batches, Eval::kkp[k0][k1][p][weight_kind]);

    }
    else if (IsKkIndex(dimension_index)) {
      Square k0;
      Square k1;
      WeightKind weight_kind;
      RawIndexToKkIndex(dimension_index, k0, k1, weight_kind);
      weights[KkIndexToRawIndex(k0, k1, weight_kind)].Finalize(
        num_mini_batches, Eval::kk[k0][k1][weight_kind]);

    }
    else {
      ASSERT_LV3(false);
    }
  }

  save_eval(output_folder_path_base, 99999999999LL);
}

void Learner::error_measurement()
{
  omp_set_num_threads((int)Options["Threads"]);

  ASSERT_LV3(
    KkIndexToRawIndex(SQ_NB, SQ_ZERO, WEIGHT_KIND_ZERO) ==
    static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * WEIGHT_KIND_NB);

  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  std::unique_ptr<Learner::KifuReader> kifu_reader = std::make_unique<KifuReader>((std::string)Options["KifuDir"], false);

  Eval::load_eval();

  Search::LimitsType limits;
  limits.max_game_ply = kMaxGamePlay;
  limits.depth = 1;
  limits.silent = true;
  Search::Limits = limits;

  // 作成・破棄のコストが高いためループの外に宣言する
  std::vector<Record> records;

  auto start = std::chrono::system_clock::now();
  double sum_squared_error_of_value = 0.0;
  double sum_norm = 0.0;
  double sum_squared_error_of_winning_percentage = 0.0;
  double sum_cross_entropy = 0.0;
  for (int64_t num_processed_positions = 0; num_processed_positions < kMaxPositionsForErrorMeasurement;) {
    // 残り時間表示
    if (num_processed_positions) {
      auto current = std::chrono::system_clock::now();
      auto elapsed = current - start;
      double elapsed_sec = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
      int remaining_sec = static_cast<int>(elapsed_sec / num_processed_positions * (kMaxPositionsForErrorMeasurement - num_processed_positions));
      int h = remaining_sec / 3600;
      int m = remaining_sec / 60 % 60;
      int s = remaining_sec % 60;

      time_t     current_time;
      struct tm  *local_time;

      time(&current_time);
      local_time = localtime(&current_time);
      std::printf("%I64d / %I64d (%04d-%02d-%02d %02d:%02d:%02d remaining %02d:%02d:%02d)\n",
        num_processed_positions, kMaxPositionsForErrorMeasurement,
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
        local_time->tm_hour, local_time->tm_min, local_time->tm_sec, h, m, s);
      std::fflush(stdout);
    }

    int num_records = static_cast<int>(std::min(
      kMaxPositionsForLearning - num_processed_positions, kMiniBatchSize));
    if (!kifu_reader->Read(num_records, records)) {
      break;
    }

    // ミニバッチ
#pragma omp parallel for reduction(+:sum_squared_error_of_value) reduction(+:sum_norm) reduction(+:sum_squared_error_of_winning_percentage) reduction(+:sum_cross_entropy)
    for (int record_index = 0; record_index < num_records; ++record_index) {
      int thread_index = omp_get_thread_num();
      Thread& thread = *Threads[thread_index];
      Position& pos = thread.rootPos;
      pos.set_this_thread(&thread);

      pos.set(records[record_index].sfen);
      Value record_value = records[record_index].value;

      Value value;
      Color rootColor;
      pos.set_this_thread(&thread);
      if (!search_shallowly(pos, value, rootColor)) {
        continue;
      }

      double diff_value = record_value - value;
      sum_squared_error_of_value += diff_value * diff_value;
      double p = winning_percentage(record_value);
      double q = winning_percentage(value);
      double diff_winning_percentage = p - q;
      sum_squared_error_of_winning_percentage += diff_winning_percentage * diff_winning_percentage;
      sum_cross_entropy += (-p * std::log(q + kEps) - (1.0 - p) * std::log(1.0 - q + kEps));
      sum_norm += abs(value);
    }

    num_processed_positions += num_records;
  }

  std::printf(
    "info string rmse_value=%f rmse_winning_percentage=%f mean_cross_entropy=%f norm=%f\n",
    std::sqrt(sum_squared_error_of_value / kMaxPositionsForErrorMeasurement),
    std::sqrt(sum_squared_error_of_winning_percentage / kMaxPositionsForErrorMeasurement),
    sum_cross_entropy / kMaxPositionsForErrorMeasurement,
    sum_norm / kMaxPositionsForErrorMeasurement);
  std::fflush(stdout);
}

void Learner::kifu_reader_benchmark()
{
  std::unique_ptr<Learner::KifuReader> kifu_reader = std::make_unique<Learner::KifuReader>((std::string)Options["KifuDir"], true);
  auto start = std::chrono::system_clock::now();

  std::vector<Record> records;
  for (int64_t num_processed_positions = 0; num_processed_positions < kMaxPositionsForLearning;) {
    // 残り時間表示
    if (num_processed_positions) {
      auto current = std::chrono::system_clock::now();
      auto elapsed = current - start;
      double elapsed_sec = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
      int remaining_sec = static_cast<int>(elapsed_sec / num_processed_positions * (kMaxPositionsForLearning - num_processed_positions));
      int h = remaining_sec / 3600;
      int m = remaining_sec / 60 % 60;
      int s = remaining_sec % 60;

      time_t     current_time;
      struct tm  *local_time;

      time(&current_time);
      local_time = localtime(&current_time);
      std::printf("%I64d / %I64d (%04d-%02d-%02d %02d:%02d:%02d remaining %02d:%02d:%02d)\n",
        num_processed_positions, kMaxPositionsForLearning,
        local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
        local_time->tm_hour, local_time->tm_min, local_time->tm_sec, h, m, s);
      std::fflush(stdout);
    }

    int num_records = static_cast<int>(std::min(
      kMaxPositionsForLearning - num_processed_positions, kMiniBatchSize));
    if (!kifu_reader->Read(num_records, records)) {
      break;
    }

    num_processed_positions += num_records;
  }
}
