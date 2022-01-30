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
    // elmo(WCSC27)�Ŏg���Ă���萔�B�v�����B
    // elmo�̂ق��͎���������Ă��Ȃ��̂Œl���Ⴄ�B
    // learn�R�}���h�ł��̒l��ݒ�ł���B
    // 0.33�́Aelmo(WCSC27)�Ŏg���Ă����萔(0.5)����
    double ELMO_LAMBDA = 0.33;
    double ELMO_LAMBDA2 = 0.33;
    double ELMO_LAMBDA_LIMIT = 32000;
    constexpr const double epsilon = 0.000001;
    double winning_percentage_for_win = 1.0;

    // ���ʂ̃V�O���C�h�֐�
    double sigmoid(double x)
    {
        return 1.0 / (1.0 + std::exp(-x));
    }

    // �]���l������[0,1]�ɕϊ�����֐�
    double winning_percentage(double value)
    {
        // ����600.0�Ƃ����萔�́Aponanza�萔�B(ponanza���������Ă���炵���Ƃ����Ӗ���)
        // �Q�[���̐i�s�x�ɍ��킹�����̂ɂ����ق������������m��Ȃ����ǂ��A���̌��ʂ̂قǂ͕s���B
        return sigmoid(value / 600.0);
    }

    // �w�K���̌����G���g���s�[�̌v�Z
    // elmo���̏��s���Ə������Ƃ̌ʂ̌����G���g���s�[�������ł���cross_entropy_eval��cross_entropy_win�ɕԂ�B
    // �i�s�x�ɂ��w�K���̒����ɂ��Ă��l������B
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

        // �[���T���ł̕]���l��ELMO_LAMBDA_LIMIT�𒴂��Ă���Ȃ�ELMO_LAMBDA�ł͂Ȃ�ELMO_LAMBDA2��K�p����B
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
            // �����̃v���Z�X��learn�R�}���h�����s�����ꍇ�ANUMA�m�[�h0�����g���Ȃ��Ȃ���ւ̑Ώ�
            WinProcGroup::bindThisThread(thread_index);

#pragma omp for schedule(dynamic, 512) reduction(+:sum_cross_entropy_eval, sum_cross_entropy_win, sum_cross_entropy, sum_entropy_eval, sum_entropy_win, sum_entropy, sum_norm, sum_move_accord_ratio)
            for (int packed_sfen_value_index = 0; packed_sfen_value_index < num_packed_sfen_values; ++packed_sfen_value_index) {
                const auto& ps = packed_sfen_values[packed_sfen_value_index];
                auto th = Threads[thread_index];
                auto& pos = th->rootPos;
                StateInfo si;
                if (pos.set_from_packed_sfen(ps.sfen, &si, th).is_not_ok())
                {
                    // �^����rmse�v�Z�p��sfen�Ƃ��āA�s����sfen�������Ă��܂��Ă����B
                    std::cout << "Error! : illegal packed sfen " << pos.sfen() << std::endl;
                }

                // ���t�̎w����Ɛ󂢒T���̃X�R�A����v���邩�̔���
                auto r = Learner::search(pos, 1);
                if ((u16)r.second[0] == ps.move)
                    ++sum_move_accord_ratio;

                // �󂢒T���̕]���l
                // evaluate()�̒l��p���Ă��ǂ��̂����A���X���v�Z����Ƃ���learn_cross_entropy��
                // �l����r���ɂ����č���̂�qsearch()��p����B
                // EvalHash�͎��O�ɖ��������Ă���B(�������Ȃ��Ɩ��񓯂��l���Ԃ��Ă��܂�)
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

                // �[���T���̕]���l
                auto deep_value = (Value)ps.score;

                // ��) ���̃R�[�h�́Alearn�R�}���h��eval_limit���w�肵�Ă���Ƃ��̂��Ƃ��l�����ĂȂ��B

                // --- �����G���g���s�[�̌v�Z

                // �Ƃ肠����elmo method�̎������������Ə��s���Ɋւ���
                // �����G���g���s�[���v�Z���ĕ\��������B

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
        // �ۑ��O��check sum���v�Z���ďo�͂��Ă����B(���ɓǂݍ��񂾂Ƃ��ɍ��v���邩���ׂ邽��)
        std::cout << "Check Sum = " << std::hex << Eval::calc_check_sum() << std::dec << std::endl;

        // �ۑ����ƂɃt�@�C�����̊g���q������"0","1","2",..�̂悤�ɕς��Ă����B
        // (���Ƃł��ꂼ��̕]���֐��p�����[�^�[�ɂ����ď������r����������)

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

    // mini_batch_size �f�t�H���g��1M�ǖʁB�����傫���ł���B
    auto mini_batch_size = 1000000;

    // ���[�v��(���̉񐔂��������t�@�C����ǂݍ���)
    int loop = 1;

    // �w�K�f�[�^�t�H���_�p�X
    std::string training_data_folder_path;

    // ���؃f�[�^�t�H���_�p�X
    std::string validation_data_folder_path;

    // 0�ł���΁A�f�t�H���g�l�ɂȂ�B
    double eta1 = 0.0;
    double eta2 = 0.0;
    double eta3 = 0.0;
    u64 eta1_epoch = 0; // default�ł�eta2�͓K�p����Ȃ�
    u64 eta2_epoch = 0; // default�ł�eta3�͓K�p����Ȃ�

#if defined(USE_GLOBAL_OPTIONS)
    // ���Ƃŕ������邽�߂ɕۑ����Ă����B
    auto old_global_options = GlobalOptions;
    // eval hash��hit�����rmse�Ȃǂ̌v�Z���ł��Ȃ��Ȃ�̂ŃI�t�ɂ��Ă����B
    GlobalOptions.use_eval_hash = false;
    // �u���\��hit����Ƃ����ňȑO�̕]���l�Ŏ}���肪����邱�Ƃ�����̂ŃI�t�ɂ��Ă����B
    GlobalOptions.use_hash_probe = false;
#endif

    // ���t�ǖʂ̐[���T���ł̕]���l�̐�Βl���A���̒l�𒴂��Ă����炻�̋ǖʂ͎̂Ă�B
    int eval_limit = 32000;

    // elmo lambda
    ELMO_LAMBDA = 0.33;
    ELMO_LAMBDA2 = 0.33;
    ELMO_LAMBDA_LIMIT = 32000;

    // if (gamePly < rand(reduction_gameply)) continue;
    // �̂悤�ɂ��āA���Ղ��w�K�Ώۂ�����悭���O���邽�߂̃I�v�V����
    // 1�ɂ��Ă����rand(1)==0�Ȃ̂ŁA�������O����Ȃ��B
    int reduction_gameply = 1;

    u64 nn_batch_size = 1000;
    double newbob_decay = 1.0;
    int newbob_num_trials = 2;
    std::string nn_options;

    u64 eval_save_interval = 1000000000ULL;
    u64 loss_output_interval = 0;
    u64 mirror_percentage = 0;

    // �t�@�C���������ɂ��炸��Ə�����Ă���Ɖ��肵�Ă���B
    while (true)
    {
        std::string option;
        is >> option;

        if (option == "")
            break;

        // mini-batch�̋ǖʐ����w��
        if (option == "bat")
        {
            is >> mini_batch_size;
            mini_batch_size *= 10000; // �P�ʂ͖�
        }

        // �������i�[����Ă���t�H���_���w�肵�āA���������ΏۂƂ���B
        else if (option == "training_data_folder_path") is >> training_data_folder_path;

        // ���[�v�񐔂̎w��
        else if (option == "loop")      is >> loop;

        // �~�j�o�b�`�̃T�C�Y
        else if (option == "batchsize") is >> mini_batch_size;

        // �w�K��
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

    // OpenMP�����Ȃ�x�����o���悤�ɁB
#if !defined(_OPENMP)
    std::cout << "Warning! OpenMP disabled." << std::endl;
#endif

    // �w�K�����t�@�C���̕\��
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

    // reduction_gameply��0��ݒ肳����rand(0)��0���Z�ɂȂ��Ă��܂��̂�1�ɕ␳�B
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
    //            �e�평����
    // -----------------------------------

    std::cout << "init.." << std::endl;

    // �]���֐��p�����[�^�[�̓ǂݍ���
    is_ready(true);

    std::cout << "init_training.." << std::endl;
    Eval::NNUE::InitializeTraining(eta1, eta1_epoch, eta2, eta2_epoch, eta3);
    Eval::NNUE::SetBatchSize(mini_batch_size);
    Eval::NNUE::SetOptions(nn_options);
    // ����܂łɍł����؃��X�����������]���֐����܂܂��t�H���_�p�X�B
    std::string best_nn_directory;
    if (newbob_decay != 1.0 && !Options["SkipLoadingEval"]) {
        best_nn_directory = std::string(Options["EvalDir"]);
    }

    std::cout << "init done." << std::endl;

    // ���̑��A�I�v�V�����ݒ�𔽉f������B
    double newbob_scale = 1.0;

    // ���؃f�[�^��ǂݍ���
    std::vector<Learner::PackedSfenValue> validation_data;
    Tanuki::KifuReader validation_data_reader(validation_data_folder_path, 1);
    Learner::PackedSfenValue packed_sfen_value;
    while (validation_data_reader.Read(packed_sfen_value)) {
        validation_data.push_back(packed_sfen_value);
    }

    // ���̎��_�ň�x���؃��X���v�Z����B
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
    //   �]���֐��p�����[�^�[�̊w�K�̊J�n
    // -----------------------------------

    Tanuki::KifuReader training_data_reader(training_data_folder_path, loop, num_threads);

    // �w�K�J�n�B
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
            // mini_batch_size��Eval::NNUE::AddExample()����
            int thread_index = omp_get_thread_num();
            auto th = Threads[thread_index];
            auto& pos = th->rootPos;

            // �����̃v���Z�X��learn�R�}���h�����s�����ꍇ�ANUMA�m�[�h0�����g���Ȃ��Ȃ���ւ̑Ώ�
            WinProcGroup::bindThisThread(thread_index);

#pragma omp for schedule(dynamic, 512) reduction(+:learn_sum_cross_entropy_eval, learn_sum_cross_entropy_win, learn_sum_cross_entropy, learn_sum_entropy_eval, learn_sum_entropy_win, learn_sum_entropy)
            for (int sample_index = 0; sample_index < mini_batch_size; ++sample_index) {
                // goto�D���ł͂Ȃ��̂ŁAfor(;;)��continue�ő�p����B
                for (;;) {
                    Learner::PackedSfenValue ps;
                    training_data_reader.Read(ps, thread_index);

                    // �]���l���w�K�Ώۂ̒l�𒴂��Ă���B
                    // ���̋ǖʏ��𖳎�����B
                    if (eval_limit < abs(ps.score) || abs(ps.score) == VALUE_SUPERIOR)
                        continue;

                    // ���ՋǖʂɊւ���ǂݔ�΂�
                    if (ps.gamePly < reduction_gameply_distribution(mt))
                        continue;

                    StateInfo si;
                    const bool mirror = mirror_percentage_distribution(mt) < mirror_percentage;
                    if (pos.set_from_packed_sfen(ps.sfen, &si, th, mirror).is_not_ok())
                    {
                        // �ς�sfen��͂��܂��ꂽ�B�f�o�b�O���ׂ��I
                        // �s����sfen�Ȃ̂�pos.sfen()�ŕ\���ł���Ƃ͌���Ȃ����A���Ȃ����}�V�B
                        std::cout << "Error! : illigal packed sfen = " << pos.sfen() << std::endl;
                        continue;
                    }

                    // �S���ċl��ł���\��������B
                    // �܂��錾�����̋ǖʂ�PV�̎w�����leaf�ɍs���Ȃ��̂Ŋw�K���珜�O���Ă����B
                    // (���̂悤�ȋ��t�ǖʎ��̂������o���ׂ��ł͂Ȃ��̂����Â��������[�`���ŏ����o���Ă��邩���m��Ȃ��̂�)
                    if (pos.is_mated() || pos.DeclarationWin() != MOVE_NONE)
                        continue;

                    // �ǂݍ��߂��̂Ŏ����ɕ\�����Ă݂�B
                    //		cout << pos << value << endl;

                    // �󂢒T��(qsearch)�̕]���l
                    auto r = Learner::qsearch(pos);

                    auto pv = r.second;

                    // �[���T���̕]���l
                    auto deep_value = (Value)ps.score;

                    // mini batch�̂ق������z���o�Ă����悤�ȋC������B
                    // ���̂܂�leaf node�ɍs���āA���z�z��ɂ��������Ă����A���Ƃ�rmse�̏W�v�̂Ƃ���AdaGrad���Ă݂�B

                    auto rootColor = pos.side_to_move();

                    // PV�̏��肪�قȂ�ꍇ�͊w�K�ɗp���Ȃ��ق����ǂ��̂ł́c�B
                    // �S�R�Ⴄ�Ƃ����T���������ʂ��Ƃ��ꂪ�m�C�Y�ɐ��肩�˂Ȃ��B
                    // �]���l�̍����傫������Ƃ�����w�K�ΏۂƂ��Ȃ��ق������������c�B

                    int ply = 0;

                    StateInfo state[MAX_PLY]; // qsearch��PV������Ȃɒ����Ȃ邱�Ƃ͂��肦�Ȃ��B
                    for (auto m : pv)
                    {
                        // �񍇖@��͂���Ă��Ȃ��͂��Ȃ̂����B
                        if (!pos.pseudo_legal(m) || !pos.legal(m))
                        {
                            std::cout << pos << m << std::endl;
                            ASSERT_LV3(false);
                        }

                        pos.do_move(m, state[ply++]);

                        // leaf�ł�evaluate�̒l��p����̂ō����X�V���Ă����B
                        Eval::evaluate_with_no_return(pos);
                    }

                    // PV�̏I�[�ǖʂɒB�����̂ŁA�����Ō��z�����Z����B

                    // shallow_value�Ƃ��āAleaf�ł�evaluate�̒l��p����B
                    // qsearch()�̖߂�l��shallow_value�Ƃ��ėp����ƁA
                    // PV���r���œr�؂�Ă���ꍇ�A���z���v�Z����̂�evaluate()���Ăяo�����ǖʂƁA
                    // ���̌��z��^����ǖʂƂ��قȂ邱�ƂɂȂ�̂ŁA����͂��܂�D�܂��������ł͂Ȃ��Ǝv���B
                    // �u���\���I�t�ɂ͂��Ă���̂����A1��l�݂Ȃǂ�pv�z����X�V���Ă��Ȃ��̂Łc�B

                    Value shallow_value = (rootColor == pos.side_to_move()) ? Eval::evaluate(pos) : -Eval::evaluate(pos);

                    // �w�K�f�[�^�ɑ΂��郍�X�̌v�Z
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

                    // �w�K�f�[�^����������I�����̂ŁAfor(;;)�ɂ��U�������[�v�𔲂���B
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
    // GlobalOptions�̕����B
    GlobalOptions = old_global_options;
#endif

}

#endif
