#ifdef EVAL_LEARN

#include <chrono>
#include <random>
#include <sstream>

#include <omp.h>

#include "types.h"
#include "config.h"

#include "eval/evaluate_common.h"
#include "eval/nnue/evaluate_nnue_learner.h"
#include "misc.h"
#include "tanuki_kifu_reader.h"
#include "tanuki_trainer.h"
#include "thread.h"

namespace {
    // elmo(WCSC27)で使われている定数。要調整。
    // elmoのほうは式を内分していないので値が違う。
    // learnコマンドでこの値を設定できる。
    // 0.33は、elmo(WCSC27)で使われていた定数(0.5)相当
    double ELMO_LAMBDA = 0.33;
    double ELMO_LAMBDA2 = 0.33;
    double ELMO_LAMBDA_LIMIT = 32000;
    constexpr const double epsilon = 0.000001;
    double winning_percentage_for_win = 1.0;

    // 普通のシグモイド関数
    double sigmoid(double x)
    {
        return 1.0 / (1.0 + std::exp(-x));
    }

    // 評価値を勝率[0,1]に変換する関数
    double winning_percentage(double value)
    {
        // この600.0という定数は、ponanza定数。(ponanzaがそうしているらしいという意味で)
        // ゲームの進行度に合わせたものにしたほうがいいかも知れないけども、その効果のほどは不明。
        return sigmoid(value / 600.0);
    }

    // 学習時の交差エントロピーの計算
    // elmo式の勝敗項と勝率項との個別の交差エントロピーが引数であるcross_entropy_evalとcross_entropy_winに返る。
    // 進行度による学習率の調整についても考慮する。
    void calc_cross_entropy(Value deep, Value shallow, const Learner::PackedSfenValue& psv,
        double& cross_entropy_eval, double& cross_entropy_win, double& cross_entropy,
        double& entropy_eval, double& entropy_win, double& entropy)
    {
        const double p /* teacher_winrate */ = winning_percentage(deep);
        const double q /* eval_winrate    */ = winning_percentage(shallow);
        const double t =
            psv.game_result == 1 ? winning_percentage_for_win :
            psv.game_result == 0 ? 0.5 :
            1.0 - winning_percentage_for_win;

        // 深い探索での評価値がELMO_LAMBDA_LIMITを超えているならELMO_LAMBDAではなくELMO_LAMBDA2を適用する。
        const double lambda = (abs(deep) >= ELMO_LAMBDA_LIMIT) ? ELMO_LAMBDA2 : ELMO_LAMBDA;

        const double m = (1.0 - lambda) * t + lambda * p;

        cross_entropy_eval =
            (-p * std::log(q + epsilon) - (1.0 - p) * std::log(1.0 - q + epsilon));
        cross_entropy_win =
            (-t * std::log(q + epsilon) - (1.0 - t) * std::log(1.0 - q + epsilon));
        entropy_eval =
            (-p * std::log(p + epsilon) - (1.0 - p) * std::log(1.0 - p + epsilon));
        entropy_win =
            (-t * std::log(t + epsilon) - (1.0 - t) * std::log(1.0 - t + epsilon));

        cross_entropy =
            (-m * std::log(q + epsilon) - (1.0 - m) * std::log(1.0 - q + epsilon));
        entropy =
            (-m * std::log(m + epsilon) - (1.0 - m) * std::log(1.0 - m + epsilon));
    }

    void MeasureTestLoss(const std::vector<Learner::PackedSfenValue>& packed_sfen_values,
        double& cross_entropy_eval, double& cross_entropy_win, double& cross_entropy,
        double& entropy_eval, double& entropy_win, double& entropy, double& norm, double& move_accord_ratio) {
        double sum_cross_entropy_eval = 0.0;
        double sum_cross_entropy_win = 0.0;
        double sum_cross_entropy = 0.0;
        double sum_entropy_eval = 0.0;
        double sum_entropy_win = 0.0;
        double sum_entropy = 0.0;
        double sum_norm = 0.0;
        double sum_move_accord_ratio = 0.0;

        int num_packed_sfen_values = packed_sfen_values.size();

#pragma omp parallel
        {
            int thread_index = omp_get_thread_num();
            // 複数のプロセスでlearnコマンドを実行した場合、NUMAノード0しか使われなくなる問題への対処
            WinProcGroup::bindThisThread(thread_index);

#pragma omp for schedule(dynamic, 512) reduction(+:sum_cross_entropy_eval, sum_cross_entropy_win, sum_cross_entropy, sum_entropy_eval, sum_entropy_win, sum_entropy, sum_norm, sum_move_accord_ratio)
            for (int packed_sfen_value_index = 0; packed_sfen_value_index < num_packed_sfen_values; ++packed_sfen_value_index) {
                const auto& ps = packed_sfen_values[packed_sfen_value_index];
                auto th = Threads[thread_index];
                auto& pos = th->rootPos;
                StateInfo si;
                if (pos.set_from_packed_sfen(ps.sfen, &si, th).is_not_ok())
                {
                    // 運悪くrmse計算用のsfenとして、不正なsfenを引いてしまっていた。
                    std::cout << "Error! : illegal packed sfen " << pos.sfen() << std::endl;
                }

                // 教師の指し手と浅い探索のスコアが一致するかの判定
                auto r = Learner::search(pos, 1);
                if ((u16)r.second[0] == ps.move)
                    ++sum_move_accord_ratio;

                // 浅い探索の評価値
                // evaluate()の値を用いても良いのだが、ロスを計算するときにlearn_cross_entropyと
                // 値が比較しにくくて困るのでqsearch()を用いる。
                // EvalHashは事前に無効化してある。(そうしないと毎回同じ値が返ってしまう)
                r = Learner::qsearch(pos);

                auto shallow_value = r.first;
                const auto rootColor = pos.side_to_move();
                const auto pv = r.second;
                StateInfo states[MAX_PLY];
                for (size_t i = 0; i < pv.size(); ++i)
                {
                    pos.do_move(pv[i], states[i]);
                    Eval::evaluate_with_no_return(pos);
                }
                shallow_value = (rootColor == pos.side_to_move()) ? Eval::evaluate(pos) : -Eval::evaluate(pos);

                // 深い探索の評価値
                auto deep_value = (Value)ps.score;

                // 注) このコードは、learnコマンドでeval_limitを指定しているときのことを考慮してない。

                // --- 交差エントロピーの計算

                // とりあえずelmo methodの時だけ勝率項と勝敗項に関して
                // 交差エントロピーを計算して表示させる。

                double local_cross_entropy_eval, local_cross_entropy_win, local_cross_entropy;
                double local_entropy_eval, local_entropy_win, local_entropy;
                calc_cross_entropy(deep_value, shallow_value, ps, local_cross_entropy_eval, local_cross_entropy_win, local_cross_entropy, local_entropy_eval, local_entropy_win, local_entropy);
                sum_cross_entropy_eval += local_cross_entropy_eval;
                sum_cross_entropy_win += local_cross_entropy_win;
                sum_cross_entropy += local_cross_entropy;
                sum_entropy_eval += local_entropy_eval;
                sum_entropy_win += local_entropy_win;
                sum_entropy += local_entropy;
                sum_norm += (double)abs(shallow_value);
            }
        }

        cross_entropy_eval = sum_cross_entropy_eval / num_packed_sfen_values;
        cross_entropy_win = sum_cross_entropy_win / num_packed_sfen_values;
        cross_entropy = sum_cross_entropy / num_packed_sfen_values;
        entropy_eval = sum_entropy_eval / num_packed_sfen_values;
        entropy_win = sum_entropy_win / num_packed_sfen_values;
        entropy = sum_entropy / num_packed_sfen_values;
        norm = sum_norm / num_packed_sfen_values;
        move_accord_ratio = sum_move_accord_ratio / num_packed_sfen_values;
    }

    bool Save(bool is_final, const double newbob_decay, const int newbob_num_trials,
        double& best_loss, double& latest_loss_sum, int64_t& latest_loss_count, std::string& best_nn_directory, double& newbob_scale) {
        // 保存前にcheck sumを計算して出力しておく。(次に読み込んだときに合致するか調べるため)
        std::cout << "Check Sum = " << std::hex << Eval::calc_check_sum() << std::dec << std::endl;

        // 保存ごとにファイル名の拡張子部分を"0","1","2",..のように変えていく。
        // (あとでそれぞれの評価関数パラメーターにおいて勝率を比較したいため)

        if (is_final) {
            Eval::save_eval("final");
            return true;
        }

        static int dir_number = 0;
        const std::string dir_name = std::to_string(dir_number++);
        Eval::save_eval(dir_name);
        if (newbob_decay != 1.0 && latest_loss_count > 0) {
            static int trials = newbob_num_trials;
            const double latest_loss = latest_loss_sum / latest_loss_count;
            latest_loss_sum = 0.0;
            latest_loss_count = 0;
            std::cout << "loss: " << latest_loss;
            if (latest_loss < best_loss) {
                std::cout << " < best (" << best_loss << "), accepted" << std::endl;
                best_loss = latest_loss;
                best_nn_directory = Path::Combine((std::string)Options["EvalSaveDir"], dir_name);
                trials = newbob_num_trials;
            }
            else {
                std::cout << " >= best (" << best_loss << "), rejected" << std::endl;
                if (best_nn_directory.empty()) {
                    std::cout << "WARNING: no improvement from initial model" << std::endl;
                }
                else {
                    std::cout << "restoring parameters from " << best_nn_directory << std::endl;
                    Eval::NNUE::RestoreParameters(best_nn_directory);
                }
                if (--trials > 0 && !is_final) {
                    std::cout << "reducing learning rate scale from " << newbob_scale
                        << " to " << (newbob_scale * newbob_decay)
                        << " (" << trials << " more trials)" << std::endl;
                    newbob_scale *= newbob_decay;
                    Eval::NNUE::SetGlobalLearningRateScale(newbob_scale);
                }
            }
            if (trials == 0) {
                std::cout << "converged" << std::endl;
                return true;
            }
        }

        return false;
    }
}

void Tanuki::InitializeTrainer(USI::OptionsMap& o)
{
}

void Tanuki::Train(std::istringstream& is)
{
    auto num_threads = (int)Options["Threads"];
    omp_set_num_threads(num_threads);

    // mini_batch_size デフォルトで1M局面。これを大きくできる。
    auto mini_batch_size = 1000000;

    // ループ回数(この回数だけ棋譜ファイルを読み込む)
    int loop = 1;

    // 学習データフォルダパス
    std::string training_data_folder_path;

    // 検証データフォルダパス
    std::string validation_data_folder_path;

    // 0であれば、デフォルト値になる。
    double eta1 = 0.0;
    double eta2 = 0.0;
    double eta3 = 0.0;
    u64 eta1_epoch = 0; // defaultではeta2は適用されない
    u64 eta2_epoch = 0; // defaultではeta3は適用されない

#if defined(USE_GLOBAL_OPTIONS)
    // あとで復元するために保存しておく。
    auto old_global_options = GlobalOptions;
    // eval hashにhitするとrmseなどの計算ができなくなるのでオフにしておく。
    GlobalOptions.use_eval_hash = false;
    // 置換表にhitするとそこで以前の評価値で枝刈りがされることがあるのでオフにしておく。
    GlobalOptions.use_hash_probe = false;
#endif

    // 教師局面の深い探索での評価値の絶対値が、この値を超えていたらその局面は捨てる。
    int eval_limit = 32000;

    // elmo lambda
    ELMO_LAMBDA = 0.33;
    ELMO_LAMBDA2 = 0.33;
    ELMO_LAMBDA_LIMIT = 32000;

    // if (gamePly < rand(reduction_gameply)) continue;
    // のようにして、序盤を学習対象から程よく除外するためのオプション
    // 1にしてあるとrand(1)==0なので、何も除外されない。
    int reduction_gameply = 1;

    u64 nn_batch_size = 1000;
    double newbob_decay = 1.0;
    int newbob_num_trials = 2;
    std::string nn_options;

    u64 eval_save_interval = 1000000000ULL;
    u64 loss_output_interval = 0;
    u64 mirror_percentage = 0;

    // ファイル名が後ろにずらずらと書かれていると仮定している。
    while (true)
    {
        std::string option;
        is >> option;

        if (option == "")
            break;

        // mini-batchの局面数を指定
        if (option == "bat")
        {
            is >> mini_batch_size;
            mini_batch_size *= 10000; // 単位は万
        }

        // 棋譜が格納されているフォルダを指定して、根こそぎ対象とする。
        else if (option == "training_data_folder_path") is >> training_data_folder_path;

        // ループ回数の指定
        else if (option == "loop")      is >> loop;

        // ミニバッチのサイズ
        else if (option == "batchsize") is >> mini_batch_size;

        // 学習率
        else if (option == "eta")        is >> eta1;
        else if (option == "eta1")       is >> eta1; // alias
        else if (option == "eta2")       is >> eta2;
        else if (option == "eta3")       is >> eta3;
        else if (option == "eta1_epoch") is >> eta1_epoch;
        else if (option == "eta2_epoch") is >> eta2_epoch;

        // LAMBDA
        else if (option == "lambda")       is >> ELMO_LAMBDA;
        else if (option == "lambda2")      is >> ELMO_LAMBDA2;
        else if (option == "lambda_limit") is >> ELMO_LAMBDA_LIMIT;

        else if (option == "reduction_gameply") is >> reduction_gameply;

        else if (option == "eval_limit") is >> eval_limit;

        else if (option == "nn_batch_size") is >> nn_batch_size;
        else if (option == "newbob_decay") is >> newbob_decay;
        else if (option == "newbob_num_trials") is >> newbob_num_trials;
        else if (option == "nn_options") is >> nn_options;
        else if (option == "eval_save_interval") is >> eval_save_interval;
        else if (option == "loss_output_interval") is >> loss_output_interval;
        else if (option == "mirror_percentage") is >> mirror_percentage;
        else if (option == "validation_data_folder_path") is >> validation_data_folder_path;
        else if (option == "winning_percentage_for_win") is >> winning_percentage_for_win;

        else {
            std::cout << "Invalid argument: " << option << std::endl;
            return;
        }

    }
    if (loss_output_interval == 0)
        loss_output_interval = mini_batch_size;

    std::cout << "learn command , ";

    // OpenMP無効なら警告を出すように。
#if !defined(_OPENMP)
    std::cout << "Warning! OpenMP disabled." << std::endl;
#endif

    // 学習棋譜ファイルの表示
    std::cout << "training_data_folder_path: " << training_data_folder_path << std::endl;
    std::cout << "validation_data_folder_path: " << validation_data_folder_path << std::endl;

    std::cout << "loop              : " << loop << std::endl;
    std::cout << "eval_limit        : " << eval_limit << std::endl;

    std::cout << "Loss Function     : " << "ELMO_METHOD(WCSC27)" << std::endl;
    std::cout << "mini-batch size   : " << mini_batch_size << std::endl;
    std::cout << "nn_batch_size     : " << nn_batch_size << std::endl;
    std::cout << "nn_options        : " << nn_options << std::endl;
    std::cout << "learning rate     : " << eta1 << " , " << eta2 << " , " << eta3 << std::endl;
    std::cout << "eta_epoch         : " << eta1_epoch << " , " << eta2_epoch << std::endl;
    if (newbob_decay != 1.0) {
        std::cout << "scheduling        : newbob with decay = " << newbob_decay
            << ", " << newbob_num_trials << " trials" << std::endl;
    }
    else {
        std::cout << "scheduling        : default" << std::endl;
    }

    // reduction_gameplyに0を設定されるとrand(0)が0除算になってしまうので1に補正。
    reduction_gameply = std::max(reduction_gameply, 1);
    std::cout << "reduction_gameply : " << reduction_gameply << std::endl;

    std::cout << "LAMBDA            : " << ELMO_LAMBDA << std::endl;
    std::cout << "LAMBDA2           : " << ELMO_LAMBDA2 << std::endl;
    std::cout << "LAMBDA_LIMIT      : " << ELMO_LAMBDA_LIMIT << std::endl;
    std::cout << "mirror_percentage : " << mirror_percentage << std::endl;
    std::cout << "eval_save_interval  : " << eval_save_interval << " sfens" << std::endl;
    std::cout << "loss_output_interval: " << loss_output_interval << " sfens" << std::endl;
    std::cout << "winning_percentage_for_win: " << winning_percentage_for_win << std::endl;
    // -----------------------------------
    //            各種初期化
    // -----------------------------------

    std::cout << "init.." << std::endl;

    // 評価関数パラメーターの読み込み
    is_ready(true);

    std::cout << "init_training.." << std::endl;
    Eval::NNUE::InitializeTraining(eta1, eta1_epoch, eta2, eta2_epoch, eta3);
    Eval::NNUE::SetBatchSize(mini_batch_size);
    Eval::NNUE::SetOptions(nn_options);
    // これまでに最も検証ロスが下がった評価関数が含まれるフォルダパス。
    std::string best_nn_directory;
    if (newbob_decay != 1.0 && !Options["SkipLoadingEval"]) {
        best_nn_directory = std::string(Options["EvalDir"]);
    }

    std::cout << "init done." << std::endl;

    // その他、オプション設定を反映させる。
    double newbob_scale = 1.0;

    // 検証データを読み込む
    std::vector<Learner::PackedSfenValue> validation_data;
    Tanuki::KifuReader validation_data_reader(validation_data_folder_path, 1);
    Learner::PackedSfenValue packed_sfen_value;
    while (validation_data_reader.Read(packed_sfen_value)) {
        validation_data.push_back(packed_sfen_value);
    }

    // この時点で一度検証ロスを計算する。
    double test_cross_entropy_eval = 0.0;
    double test_cross_entropy_win = 0.0;
    double test_cross_entropy = 0.0;
    double test_entropy_eval = 0.0;
    double test_entropy_win = 0.0;
    double test_entropy = 0.0;
    double test_norm = 0.0;
    double move_accord_ratio = 0.0;

    MeasureTestLoss(validation_data, test_cross_entropy_eval, test_cross_entropy_win, test_cross_entropy,
        test_entropy_eval, test_entropy_win, test_entropy, test_norm, move_accord_ratio);

    int64_t num_processed_sfens = 0;
    int iteration = 0;

    auto& pos = Threads.main()->rootPos;
    StateInfo si;
    pos.set_hirate(&si, Threads.main());

    std::cout << "PROGRESS: " << Tools::now_string()
        << " , num_processed_sfens = " << num_processed_sfens
        << " , iteration = " << iteration
        << " , eta = " << Eval::get_eta()
        << " , hirate eval = " << Eval::evaluate(pos)
        << " , test_cross_entropy_eval = " << test_cross_entropy_eval
        << " , test_cross_entropy_win = " << test_cross_entropy_win
        << " , test_entropy_eval = " << test_cross_entropy_eval
        << " , test_entropy_win = " << test_cross_entropy_win
        << " , test_cross_entropy = " << test_cross_entropy
        << " , test_entropy = " << test_entropy
        << " , test_norm = " << test_norm
        << " , move_accuracy = " << move_accord_ratio << "%"
        << std::endl;

    double best_loss = std::numeric_limits<double>::max();
    double latest_loss_sum = 0.0;
    int64_t latest_loss_count = 0;

    // -----------------------------------
    //   評価関数パラメーターの学習の開始
    // -----------------------------------

    Tanuki::KifuReader training_data_reader(training_data_folder_path, loop, num_threads);

    // 学習開始。
    std::random_device random_device;
    std::mt19937_64 mt(random_device());
    std::uniform_int_distribution<> reduction_gameply_distribution(0, reduction_gameply - 1);
    std::uniform_int_distribution<> mirror_percentage_distribution(0, 100 - 1);

    double learn_sum_cross_entropy_eval = 0.0;
    double learn_sum_cross_entropy_win = 0.0;
    double learn_sum_cross_entropy = 0.0;
    double learn_sum_entropy_eval = 0.0;
    double learn_sum_entropy_win = 0.0;
    double learn_sum_entropy = 0.0;
    int64_t last_loss_output = 0;
    int64_t last_eval_save = 0;

    for (;;) {
        Eval::NNUE::ResizeExamples(mini_batch_size);

        //auto begin_time = std::chrono::system_clock::now();

#pragma omp parallel
        {
            // mini_batch_size分Eval::NNUE::AddExample()する
            int thread_index = omp_get_thread_num();
            auto th = Threads[thread_index];
            auto& pos = th->rootPos;

            // 複数のプロセスでlearnコマンドを実行した場合、NUMAノード0しか使われなくなる問題への対処
            WinProcGroup::bindThisThread(thread_index);

#pragma omp for schedule(dynamic, 512) reduction(+:learn_sum_cross_entropy_eval, learn_sum_cross_entropy_win, learn_sum_cross_entropy, learn_sum_entropy_eval, learn_sum_entropy_win, learn_sum_entropy)
            for (int sample_index = 0; sample_index < mini_batch_size; ++sample_index) {
                // goto好きではないので、for(;;)とcontinueで代用する。
                for (;;) {
                    Learner::PackedSfenValue ps;
                    training_data_reader.Read(ps, thread_index);

                    // 評価値が学習対象の値を超えている。
                    // この局面情報を無視する。
                    if (eval_limit < abs(ps.score) || abs(ps.score) == VALUE_SUPERIOR)
                        continue;

                    // 序盤局面に関する読み飛ばし
                    if (ps.gamePly < reduction_gameply_distribution(mt))
                        continue;

                    StateInfo si;
                    const bool mirror = mirror_percentage_distribution(mt) < mirror_percentage;
                    if (pos.set_from_packed_sfen(ps.sfen, &si, th, mirror).is_not_ok())
                    {
                        // 変なsfenを掴かまされた。デバッグすべき！
                        // 不正なsfenなのでpos.sfen()で表示できるとは限らないが、しないよりマシ。
                        std::cout << "Error! : illigal packed sfen = " << pos.sfen() << std::endl;
                        continue;
                    }

                    // 全駒されて詰んでいる可能性がある。
                    // また宣言勝ちの局面はPVの指し手でleafに行けないので学習から除外しておく。
                    // (そのような教師局面自体を書き出すべきではないのだが古い生成ルーチンで書き出しているかも知れないので)
                    if (pos.is_mated() || pos.DeclarationWin() != MOVE_NONE)
                        continue;

                    // 読み込めたので試しに表示してみる。
                    //		cout << pos << value << endl;

                    // 浅い探索(qsearch)の評価値
                    auto r = Learner::qsearch(pos);

                    auto pv = r.second;

                    // 深い探索の評価値
                    auto deep_value = (Value)ps.score;

                    // mini batchのほうが勾配が出ていいような気がする。
                    // このままleaf nodeに行って、勾配配列にだけ足しておき、あとでrmseの集計のときにAdaGradしてみる。

                    auto rootColor = pos.side_to_move();

                    // PVの初手が異なる場合は学習に用いないほうが良いのでは…。
                    // 全然違うところを探索した結果だとそれがノイズに成りかねない。
                    // 評価値の差が大きすぎるところも学習対象としないほうがいいかも…。

                    int ply = 0;

                    StateInfo state[MAX_PLY]; // qsearchのPVがそんなに長くなることはありえない。
                    for (auto m : pv)
                    {
                        // 非合法手はやってこないはずなのだが。
                        if (!pos.pseudo_legal(m) || !pos.legal(m))
                        {
                            std::cout << pos << m << std::endl;
                            ASSERT_LV3(false);
                        }

                        pos.do_move(m, state[ply++]);

                        // leafでのevaluateの値を用いるので差分更新していく。
                        Eval::evaluate_with_no_return(pos);
                    }

                    // PVの終端局面に達したので、ここで勾配を加算する。

                    // shallow_valueとして、leafでのevaluateの値を用いる。
                    // qsearch()の戻り値をshallow_valueとして用いると、
                    // PVが途中で途切れている場合、勾配を計算するのにevaluate()を呼び出した局面と、
                    // その勾配を与える局面とが異なることになるので、これはあまり好ましい性質ではないと思う。
                    // 置換表をオフにはしているのだが、1手詰みなどはpv配列を更新していないので…。

                    Value shallow_value = (rootColor == pos.side_to_move()) ? Eval::evaluate(pos) : -Eval::evaluate(pos);

                    // 学習データに対するロスの計算
                    double learn_cross_entropy_eval, learn_cross_entropy_win, learn_cross_entropy;
                    double learn_entropy_eval, learn_entropy_win, learn_entropy;
                    calc_cross_entropy(deep_value, shallow_value, ps, learn_cross_entropy_eval, learn_cross_entropy_win, learn_cross_entropy, learn_entropy_eval, learn_entropy_win, learn_entropy);
                    learn_sum_cross_entropy_eval += learn_cross_entropy_eval;
                    learn_sum_cross_entropy_win += learn_cross_entropy_win;
                    learn_sum_cross_entropy += learn_cross_entropy;
                    learn_sum_entropy_eval += learn_entropy_eval;
                    learn_sum_entropy_win += learn_entropy_win;
                    learn_sum_entropy += learn_entropy;

                    Eval::NNUE::SetExample(pos, rootColor, ps, 1.0, sample_index);

                    // 学習データを一つ処理し終えたので、for(;;)による偽無限ループを抜ける。
                    break;
                }
            }
        }

        //auto end_time = std::chrono::system_clock::now();
        //std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time).count() << std::endl;

        Eval::NNUE::UpdateParameters(iteration);

        num_processed_sfens += mini_batch_size;

        if (last_loss_output + loss_output_interval <= num_processed_sfens) {
            MeasureTestLoss(validation_data, test_cross_entropy_eval, test_cross_entropy_win, test_cross_entropy,
                test_entropy_eval, test_entropy_win, test_entropy, test_norm, move_accord_ratio);

            latest_loss_sum += (test_cross_entropy - test_entropy) * validation_data.size();
            latest_loss_count += validation_data.size();

            double learn_cross_entropy_eval = learn_sum_cross_entropy_eval / (num_processed_sfens - last_loss_output);
            double learn_cross_entropy_win = learn_sum_cross_entropy_win / (num_processed_sfens - last_loss_output);
            double learn_cross_entropy = learn_sum_cross_entropy / (num_processed_sfens - last_loss_output);
            double learn_entropy_eval = learn_sum_entropy_eval / (num_processed_sfens - last_loss_output);
            double learn_entropy_win = learn_sum_entropy_win / (num_processed_sfens - last_loss_output);
            double learn_entropy = learn_sum_entropy / (num_processed_sfens - last_loss_output);

            std::cout << "PROGRESS: " << Tools::now_string()
                << " , num_processed_sfens = " << num_processed_sfens
                << " , iteration = " << iteration
                << " , eta = " << Eval::get_eta()
                << " , hirate eval = " << Eval::evaluate(pos)
                << " , test_cross_entropy_eval = " << test_cross_entropy_eval
                << " , test_cross_entropy_win = " << test_cross_entropy_win
                << " , test_entropy_eval = " << test_cross_entropy_eval
                << " , test_entropy_win = " << test_cross_entropy_win
                << " , test_cross_entropy = " << test_cross_entropy
                << " , test_entropy = " << test_entropy
                << " , test_norm = " << test_norm
                << " , move_accuracy = " << move_accord_ratio << "%"
                << " , learn_cross_entropy_eval = " << learn_cross_entropy_eval
                << " , learn_cross_entropy_win = " << learn_cross_entropy_win
                << " , learn_entropy_eval = " << learn_cross_entropy
                << " , learn_entropy_win = " << learn_entropy_eval
                << " , learn_cross_entropy = " << learn_entropy_win
                << " , learn_entropy = " << learn_entropy
                << std::endl;

            learn_sum_cross_entropy_eval = 0.0;
            learn_sum_cross_entropy_win = 0.0;
            learn_sum_cross_entropy = 0.0;
            learn_sum_entropy_eval = 0.0;
            learn_sum_entropy_win = 0.0;
            learn_sum_entropy = 0.0;
            latest_loss_count = num_processed_sfens - last_loss_output;
            last_loss_output = num_processed_sfens;
        }

        if (last_eval_save + eval_save_interval <= num_processed_sfens) {
            if (Save(false, newbob_decay, newbob_num_trials, best_loss, latest_loss_sum, latest_loss_count, best_nn_directory, newbob_scale)) {
                break;
            }

            last_eval_save = num_processed_sfens;
        }
    }

    Save(true, newbob_decay, newbob_num_trials, best_loss, latest_loss_sum, latest_loss_count, best_nn_directory, newbob_scale);

#if defined(USE_GLOBAL_OPTIONS)
    // GlobalOptionsの復元。
    GlobalOptions = old_global_options;
#endif

}

#endif
