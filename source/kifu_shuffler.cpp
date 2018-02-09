#include "kifu_shuffler.h"

#include <direct.h>
#include <cstdio>
#include <random>

#include "experimental_learner.h"
#include "kifu_reader.h"
#include "kifu_writer.h"
#include "misc.h"

namespace {
static const constexpr char* kShuffledKifuDir = "ShuffledKifuDir";
// シャッフル後のファイル数
// Windowsでは一度に512個までのファイルしか開けないため
// 256個に制限しておく
static const constexpr int kNumShuffledKifuFiles = 256;
static const constexpr int kMaxPackedSfenValues = 1024 * 1024;
static const constexpr char* kOptionNameUseDiscount = "UseDiscount";
static const constexpr char* kOptionNameUseWinningRateForDiscount = "UseWinningRateForDiscount";
static const constexpr char* kOptionNameDiscountRatio = "DiscountRatio";
static const constexpr char* kOptionNameOverwriteGameResults = "OverwriteGameResults";
static const constexpr char* kOptionNameUseSlide = "UseSlide";
static const constexpr char* kOptionNameSlideAmount = "SlideAmount";
static const constexpr char* kOptionNameUseMedianFilter = "UseMedianFilter";
static const constexpr char* kOptionNameMedianFilterRadius = "MedianFilterRadius";
static const constexpr int kMaxMedianFilterRadius = 256;

double ToScaledScore(double raw_score, bool use_winning_rate_for_discount,
                     double value_to_winning_rate_coefficient) {
    if (use_winning_rate_for_discount) {
        return Learner::ToWinningRate(raw_score, value_to_winning_rate_coefficient);
        ;
    } else {
        return raw_score;
    }
}

double ToRawScore(double scaled_score, bool use_winning_rate_for_discount,
                  double value_to_winning_rate_coefficient) {
    if (use_winning_rate_for_discount) {
        return Learner::ToValue(scaled_score, value_to_winning_rate_coefficient);
    } else {
        return scaled_score;
    }
}
}  // namespace

void Learner::InitializeKifuShuffler(USI::OptionsMap& o) {
    o[kShuffledKifuDir] << USI::Option("kifu_shuffled");
    o[kOptionNameUseDiscount] << USI::Option(false);
    o[kOptionNameUseWinningRateForDiscount] << USI::Option(false);
    o[kOptionNameOverwriteGameResults] << USI::Option(false);
    o[kOptionNameDiscountRatio] << USI::Option("0.9");
    o[kOptionNameUseSlide] << USI::Option(false);
    o[kOptionNameSlideAmount] << USI::Option(10, INT_MIN, INT_MAX);
    o[kOptionNameUseMedianFilter] << USI::Option(false);
    o[kOptionNameMedianFilterRadius] << USI::Option(3, 1, kMaxMedianFilterRadius);
}

void Learner::ShuffleKifu() {
    // 棋譜を入力し、複数のファイルにランダムに追加していく
    sync_cout << "info string Reading and dividing kifu files..." << sync_endl;

    std::string kifu_dir = Options["KifuDir"];
    std::string shuffled_kifu_dir = Options[kShuffledKifuDir];
    bool use_discount = (bool)Options[kOptionNameUseDiscount];
    bool use_winning_rate_for_discount = (bool)Options[kOptionNameUseWinningRateForDiscount];
    bool overwrite_game_results = (bool)Options[kOptionNameOverwriteGameResults];
    double value_to_winning_rate_coefficient =
        Options[kOptionValueValueToWinningRateCoefficient].cast<double>();
    double discount_ratio = Options[kOptionNameDiscountRatio].cast<double>();
    bool use_slide = (bool)Options[kOptionNameUseSlide];
    int slide_amount = (int)Options[kOptionNameSlideAmount];
    bool use_median_filter = (bool)Options[kOptionNameUseMedianFilter];
    int median_filter_radius = (int)Options[kOptionNameMedianFilterRadius];

    auto reader = std::make_unique<KifuReader>(kifu_dir, 1);
    _mkdir(shuffled_kifu_dir.c_str());

    std::vector<std::string> file_paths;
    for (int file_index = 0; file_index < kNumShuffledKifuFiles; ++file_index) {
        char file_path[_MAX_PATH];
        sprintf(file_path, "%s/shuffled.%03d.bin", shuffled_kifu_dir.c_str(), file_index);
        file_paths.push_back(file_path);
    }

    std::vector<std::shared_ptr<KifuWriter> > writers;
    for (const auto& file_path : file_paths) {
        writers.push_back(std::make_shared<KifuWriter>(file_path));
    }

    double weights[4096];
    weights[0] = 1.0;
    for (int i = 1; i < sizeof(weights) / sizeof(weights[0]); ++i) {
        weights[i] = weights[i - 1] * discount_ratio;
    }

    std::mt19937_64 mt(std::time(nullptr));
    std::uniform_int_distribution<> dist(0, kNumShuffledKifuFiles - 1);
    int64_t num_records = 0;
    for (;;) {
        std::vector<PackedSfenValue> records;
        PackedSfenValue record;
        while (reader->Read(record)) {
            records.push_back(record);
            if (record.last_position || static_cast<int>(records.size()) > kMaxPackedSfenValues) {
                break;
            }
        }

        if (records.empty()) {
            break;
        }

        if (overwrite_game_results) {
            int game_result = GameResultDraw;
            if (records.back().score > VALUE_ZERO) {
                game_result = GameResultWin;
            } else {
                game_result = GameResultLose;
            }

            for (auto rit = records.rbegin(); rit != records.rend(); ++rit) {
                rit->game_result = game_result;
                game_result = -game_result;
            }
        }

        if (use_median_filter) {
            int num_records = static_cast<int>(records.size());
            std::vector<s16> scores(num_records);
            for (int i = 0; i < num_records; ++i) {
                scores[i] = records[i].score;
            }

            // 手番から見た評価値を開始手番から見た評価値に変換する
            for (int i = 1; i < num_records; i += 2) {
                scores[i] = -scores[i];
            }

            // Median Filterを適用する
            std::vector<s16> filtered_scores(num_records);
            for (int dst = 0; dst < num_records; ++dst) {
                std::vector<s16> values;
                for (int d = -median_filter_radius; d <= median_filter_radius; ++d) {
                    int play = dst + d;
                    play = std::max(0, play);
                    play = std::min(num_records - 1, play);
                    values.push_back(scores[play]);
                }
                std::nth_element(values.begin(), values.begin() + median_filter_radius, values.end());
                filtered_scores[dst] = values[median_filter_radius];
                sync_cout << scores[dst] << "\t" << filtered_scores[dst] << sync_endl;
            }

            // 開始手番から見た評価値を手番から見た評価値に変換する
            for (int i = 1; i < num_records; i += 2) {
                filtered_scores[i] = -filtered_scores[i];
            }

            // スコアを書き戻す
            for (int i = 0; i < num_records; ++i) {
                records[i].score = filtered_scores[i];
            }

        }

        if (use_slide) {
            int num_records = static_cast<int>(records.size());
            std::vector<s16> scores(num_records);
            for (int i = 0; i < num_records; ++i) {
                scores[i] = records[i].score;
            }

            // 手番から見た評価値を開始手番から見た評価値に変換する
            for (int i = 1; i < num_records; i += 2) {
                scores[i] = -scores[i];
            }

            // スライドさせる
            std::vector<s16> slided_scores(num_records);
            for (int dst = 0; dst < num_records; ++dst) {
                int src = dst + slide_amount;
                src = std::max(0, src);
                src = std::min(num_records - 1, src);
                slided_scores[dst] = scores[src];
            }

            // 開始手番から見た評価値を手番から見た評価値に変換する
            for (int i = 1; i < num_records; i += 2) {
                slided_scores[i] = -slided_scores[i];
            }

            // スコアを書き戻す
            for (int i = 0; i < num_records; ++i) {
                records[i].score = slided_scores[i];
            }
        }

        if (use_discount) {
            // DeepStack: Expert-Level Artificial Intelligence in No-Limit Poker
            // https://arxiv.org/abs/1701.01724
            // discountを用いた重み付き評価値を計算し、元の評価値を上書きする
            // 後ろから計算すると線形時間で計算できる

            // 各局面の評価値に重みをかけたもの
            // use_winning_rate_for_discount の場合には勝率に変換した値が入る
            double weighted_scores[4096];
            int num_records = records.size();
            // 手番から見た評価値を開始手番から見た評価値に変換するための符号
            double sign = 1.0;
            for (int i = 0; i < num_records; ++i) {
                weighted_scores[i] =
                    ToScaledScore(sign * records[i].score, use_winning_rate_for_discount,
                                  value_to_winning_rate_coefficient) *
                    weights[i];
                sign = -sign;
            }

            double sum_weighted_scores = 0.0;
            double sum_weights = 0.0;
            // 一度反転すると最後の局面の符号と一致する
            sign = -sign;
            // 後ろから足していくと線形時間で計算できる
            for (int i = static_cast<int>(records.size() - 1); i >= 0; --i) {
                sum_weighted_scores += weighted_scores[i];
                sum_weights += weights[i];
                double scaled_score = sum_weighted_scores / sum_weights;
                double raw_score = ToRawScore(scaled_score, use_winning_rate_for_discount,
                                              value_to_winning_rate_coefficient);
                // std::cout << records[i].score << " -> " << sign * raw_score << std::endl;
                records[i].score = static_cast<Value>(static_cast<int>(sign * raw_score));
                sign = -sign;
            }
        }

        for (const auto& record : records) {
            if (!writers[dist(mt)]->Write(record)) {
                sync_cout << "info string Failed to write a record to a kifu file. " << sync_endl;
            }
            ++num_records;
            if (num_records % 10000000 == 0) {
                sync_cout << "info string " << num_records << sync_endl;
            }
        }
    }
    for (auto& writer : writers) {
        writer->Close();
    }

    // 各ファイルをシャッフルする
    for (const auto& file_path : file_paths) {
        sync_cout << "info string " << file_path << sync_endl;

        // ファイル全体を読み込む
        FILE* file = std::fopen(file_path.c_str(), "rb");
        if (file == nullptr) {
            sync_cout << "info string Failed to open a kifu file. " << file_path << sync_endl;
            return;
        }
        _fseeki64(file, 0, SEEK_END);
        int64_t size = _ftelli64(file);
        _fseeki64(file, 0, SEEK_SET);
        std::vector<PackedSfenValue> records(size / sizeof(PackedSfenValue));
        std::fread(&records[0], sizeof(PackedSfenValue), size / sizeof(PackedSfenValue), file);
        std::fclose(file);
        file = nullptr;

        // 棋譜全体をシャッフルする
        std::shuffle(records.begin(), records.end(), mt);

        // 棋譜全体を上書きして書き戻す
        file = std::fopen(file_path.c_str(), "wb");
        if (file == nullptr) {
            sync_cout << "info string Failed to open a kifu file. " << file_path << sync_endl;
        }
        if (std::fwrite(&records[0], sizeof(PackedSfenValue), records.size(), file) !=
            records.size()) {
            sync_cout << "info string Failed to write records to a kifu file. " << file_path
                      << sync_endl;
            return;
        }
        std::fclose(file);
        file = nullptr;
    }
}
