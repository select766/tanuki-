#include "tanuki_kifu_shuffler.h"
#include "config.h"

#ifdef EVAL_LEARN

#include <cstdio>
#include <ctime>
#include <sys/stat.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <filesystem>
#include <omp.h>
#include <random>
#include <atomic>

#include "learn/learn.h"
#include "misc.h"
#include "tanuki_kifu_reader.h"
#include "tanuki_kifu_writer.h"
#include "tanuki_progress.h"
#include "thread.h"

using Learner::PackedSfenValue;

namespace {
    static const constexpr char* kShuffledKifuDir = "ShuffledKifuDir";
    static const constexpr char* kShuffledMinPly = "ShuffledMinPly";
    static const constexpr char* kShuffledMaxPly = "ShuffledMaxPly";
    static const constexpr char* kShuffledMinProgress = "ShuffledMinProgress";
    static const constexpr char* kShuffledMaxProgress = "ShuffledMaxProgress";
    static const constexpr char* kApplyQSearch = "ApplyQSearch";
    static const constexpr char* kPairedShuffle = "PairedShuffle";
    static const constexpr char* kMaxOutputSamples = "MaxOutputSamples";
    static const constexpr char* kOffsetAlpha = "OffsetAlpha";
    static const constexpr char* kOffsetDistribution = "OffsetDistribution";
    static const constexpr char* kOffsetUniformMax = "OffsetUniformMax";
    // �V���b�t����̃t�@�C����
    // Windows�ł͈�x��512�܂ł̃t�@�C�������J���Ȃ�����
    // 256�ɐ������Ă���
    static const constexpr int kNumShuffledKifuFiles = 256;
    static const constexpr int kMaxPackedSfenValues = 1024 * 1024;

    struct PairedPackedSfenValue {
        PackedSfenValue dnn;
        PackedSfenValue nnue;
    };
    static_assert(sizeof(PairedPackedSfenValue) == 80);
}

void Tanuki::InitializeShuffler(USI::OptionsMap& o) {
    o[kShuffledKifuDir] << USI::Option("kifu_shuffled");
    o[kShuffledMinPly] << USI::Option(1, 1, std::numeric_limits<u64>::max() / 2);
    o[kShuffledMaxPly] << USI::Option(std::numeric_limits<u64>::max() / 2, 1, std::numeric_limits<u64>::max() / 2);
    o[kShuffledMinProgress] << USI::Option("0.0");
    o[kShuffledMaxProgress] << USI::Option("1.0");
    o[kApplyQSearch] << USI::Option(false);
    o[kPairedShuffle] << USI::Option(false);
    o[kMaxOutputSamples] << USI::Option(0, 0, std::numeric_limits<int>::max());
    o[kOffsetAlpha] << USI::Option("0.216");
    o[kOffsetDistribution] << USI::Option(std::vector<std::string>{"geometric", "uniform"}, "geometric");
    o[kOffsetUniformMax] << USI::Option(50, 1, std::numeric_limits<int>::max());
}

void Tanuki::ShuffleKifu(Position& position) {
    GlobalOptions_ old_global_options = GlobalOptions;
    GlobalOptions.use_eval_hash = false;
    GlobalOptions.use_hash_probe = false;

    int num_threads = (int)Options["Threads"];
    omp_set_num_threads(num_threads);

    Search::LimitsType limits;
    // ���������̎萔�t�߂ň��������̒l���Ԃ�̂�h������1 << 16�ɂ���
    limits.max_game_ply = 1 << 16;
    limits.depth = MAX_PLY;
    limits.silent = true;
    limits.enteringKingRule = EKR_27_POINT;
    Search::Limits = limits;

    // ��������͂��A�����̃t�@�C���Ƀ����_���ɒǉ����Ă���
    sync_cout << "info string Reading and dividing kifu files..." << sync_endl;

    std::string kifu_dir = Options["KifuDir"];
    std::string shuffled_kifu_dir = Options[kShuffledKifuDir];
    u64 min_ply = Options[kShuffledMinPly];
    u64 max_ply = Options[kShuffledMaxPly];
    double min_progress = std::atof(static_cast<std::string>(Options[kShuffledMinProgress]).c_str());
    double max_progress = std::atof(static_cast<std::string>(Options[kShuffledMaxProgress]).c_str());
    bool apply_qsearch = Options[kApplyQSearch];
    bool paired_shuffle = Options[kPairedShuffle];
    int64_t max_output_samples = static_cast<int64_t>(int(Options[kMaxOutputSamples]));
    if (max_output_samples < 0) {
        max_output_samples = 0;
    }
    double offset_alpha = std::atof(static_cast<std::string>(Options[kOffsetAlpha]).c_str());
    if (offset_alpha < 0.0) {
        offset_alpha = 0.0;
    }
    std::string offset_distribution = Options[kOffsetDistribution];
    if (offset_distribution != "uniform" && offset_distribution != "geometric") {
        offset_distribution = "geometric";
    }
    int offset_uniform_max = int(Options[kOffsetUniformMax]);
    if (offset_uniform_max < 1) {
        offset_uniform_max = 1;
    }
    double r = std::exp(-offset_alpha);
    if (r < 0.0) r = 0.0;
    if (r > 0.999999999) r = 0.999999999;

    sync_cout << "kifu_dir=" << kifu_dir << sync_endl;
    sync_cout << "shuffled_kifu_dir=" << shuffled_kifu_dir << sync_endl;
    sync_cout << "paired_shuffle=" << paired_shuffle << sync_endl;
    sync_cout << "max_output_samples=" << max_output_samples << sync_endl;
    sync_cout << "offset_distribution=" << offset_distribution << sync_endl;
    sync_cout << "offset_uniform_max=" << offset_uniform_max << sync_endl;
    sync_cout << "offset_alpha=" << offset_alpha << sync_endl;

    auto reader = std::make_unique<KifuReader>(kifu_dir, 1);
    mkdir(shuffled_kifu_dir.c_str(), 0755);

    std::vector<std::string> file_paths;
    for (int file_index = 0; file_index < kNumShuffledKifuFiles; ++file_index) {
        char file_path[PATH_MAX];
        sprintf(file_path, "%s/shuffled.%03d.bin", shuffled_kifu_dir.c_str(), file_index);
        file_paths.push_back(file_path);
    }

    sync_cout << "info string Opening output files..." << sync_endl;
    std::vector<std::shared_ptr<KifuWriter> > writers;
    std::vector<FILE*> paired_writers;
    if (!paired_shuffle) {
        for (const auto& file_path : file_paths) {
            writers.push_back(std::make_shared<KifuWriter>(file_path));
        }
    }
    else {
        paired_writers.reserve(file_paths.size());
        for (const auto& file_path : file_paths) {
            FILE* f = std::fopen(file_path.c_str(), "wb");
            if (f == nullptr) {
                sync_cout << "info string Failed to open paired temp output file: " << file_path << sync_endl;
                return;
            }
            if (std::setvbuf(f, nullptr, _IOFBF, std::numeric_limits<int>::max())) {
                sync_cout << "info string Failed to set output buffer for paired temp file: " << file_path << sync_endl;
                std::fclose(f);
                return;
            }
            paired_writers.push_back(f);
        }
    }

    sync_cout << "info string Starting dividing..." << sync_endl;

    std::mt19937_64 mt(std::time(nullptr));
    std::uniform_int_distribution<> dist(0, kNumShuffledKifuFiles - 1);
    std::geometric_distribution<int> offset_dist(1.0 - r);
    int64_t num_records = 0;
    u64 current_ply = 1;
    bool reached_max_output_samples = false;

    Tanuki::Progress progress_estimator;
	if (min_progress != 0.0 || max_progress != 1.0) {
		// �i�s�x���g�p���Ȃ��ꍇ�Aprogress.bin���Ȃ��Ă����s�\�ɂ���
		if (!progress_estimator.Load()) {
			sync_cout << "info string Failed to load the progress file..." << sync_endl;
			std::exit(1);
		}
	}

    for (;;) {
        std::vector<PackedSfenValue> records;
        {
            PackedSfenValue record;
            while (static_cast<int>(records.size()) < kMaxPackedSfenValues && reader->Read(record)) {
                StateInfo state_info = {};
                position.set_from_packed_sfen(record.sfen, &state_info, Threads[0]);
                double progress = 0.0;
                if (min_progress != 0.0 || max_progress != 1.0) {
                    // �������̂��߁Amin_progress�܂���max_progress���ݒ肳��Ă����ꍇ�̂�
                    // �i�s�x�𐄒肷��B
                    progress = progress_estimator.Estimate(position);
                }

                if (min_ply <= current_ply && current_ply <= max_ply &&
                    min_progress <= progress && progress <= max_progress) {
                    records.push_back(record);
                }

                // �Ō�̋ǖʂ�ǂݍ��񂾂�A�萔�����Z�b�g����B
                if (record.last_position) {
                    current_ply = 1;
                }
                else {
                    ++current_ply;
                }
            }
        }

        if (records.empty()) {
            break;
        }

        std::vector<PackedSfenValue> dnn_records;
        if (paired_shuffle) {
            dnn_records.resize(records.size());
            std::vector<int> game_start_index(records.size());
            for (int i = 0; i < static_cast<int>(records.size()); ++i) {
                if (i == 0 || records[i].gamePly != records[i - 1].gamePly + 1) {
                    game_start_index[i] = i;
                }
                else {
                    game_start_index[i] = game_start_index[i - 1];
                }
            }

            for (int i = 0; i < static_cast<int>(records.size()); ++i) {
                int max_offset = i - game_start_index[i];
                int sampled_offset = 0;
                if (offset_distribution == "uniform") {
                    if (max_offset > 0) {
                        int max_uniform_offset = std::min(offset_uniform_max, max_offset);
                        std::uniform_int_distribution<int> uniform_offset_dist(1, max_uniform_offset);
                        sampled_offset = uniform_offset_dist(mt);
                    }
                }
                else {
                    sampled_offset = offset_dist(mt);
                    if (sampled_offset > max_offset) {
                        sampled_offset = max_offset;
                    }
                }
                dnn_records[i] = records[i - sampled_offset];
            }
        }

        if (apply_qsearch) {
            std::atomic<int> global_record_index;
            global_record_index = 0;

#pragma omp parallel
            {
                int thread_index = ::omp_get_thread_num();
                WinProcGroup::bindThisThread(thread_index);
                std::vector<StateInfo> state_info(MAX_PLY);
                Thread& thread = *Threads[thread_index];
                Position& pos = thread.rootPos;
                for (int local_record_index = global_record_index++;
                    local_record_index < static_cast<int>(records.size());
                    local_record_index = global_record_index++) {
                    PackedSfenValue& record = records[local_record_index];
                    if (pos.set_from_packed_sfen(record.sfen, &state_info[0], &thread).code != Tools::Result::Ok().code) {
                        sync_cout << "Failed to call set_from_packed_sfen()." << std::endl << pos << sync_endl;
                        std::exit(1);
                    }

                    auto root_color = pos.side_to_move();

                    if (pos.this_thread() == nullptr) {
                        sync_cout << "pos.this_thread() == nullptr." << sync_endl;
                        std::exit(1);
                    }

                    auto value_and_pv = Learner::qsearch(pos);

                    int ply = 1;
                    for (auto m : value_and_pv.second)
                    {
                        ASSERT_LV3(pos.pseudo_legal(m) && pos.legal(m));
                        pos.do_move(m, state_info[ply++]);
                    }

                    pos.sfen_pack(record.sfen);

                    auto leaf_color = pos.side_to_move();

                    record.move = MOVE_NONE;

                    // Root�ǖʂƖ��[�ǖʂ̎�Ԃ��قȂ�ꍇ�A�]���l�Ə��s�𔽓]����B
                    if (root_color != leaf_color) {
                        record.score = -record.score;
                        record.game_result = -record.game_result;
                    }
                }
            }
        }

        for (int i = 0; i < static_cast<int>(records.size()); ++i) {
            if (max_output_samples > 0 && num_records >= max_output_samples) {
                reached_max_output_samples = true;
                break;
            }

            int file_index = dist(mt);
            if (!paired_shuffle) {
                const auto& record = records[i];
                if (!writers[file_index]->Write(record)) {
                    sync_cout << "info string Failed to write a record to a kifu file. " << sync_endl;
                }
            }
            else {
                PairedPackedSfenValue paired_record = {};
                paired_record.dnn = dnn_records[i];
                paired_record.nnue = records[i];
                if (std::fwrite(&paired_record, sizeof(paired_record), 1, paired_writers[file_index]) != 1) {
                    sync_cout << "info string Failed to write a paired record to a kifu file." << sync_endl;
                    reached_max_output_samples = true;
                    break;
                }
            }

            ++num_records;
            if (num_records % 10000000 == 0) {
                sync_cout << "info string " << num_records << sync_endl;
            }
        }

        if (reached_max_output_samples) {
            break;
        }
    }
    if (!paired_shuffle) {
        for (auto& writer : writers) {
            writer->Close();
        }
    }
    else {
        for (auto& writer : paired_writers) {
            std::fclose(writer);
        }
    }

    sync_cout << "info string Starting shuffling..." << sync_endl;

    // �o�̓t�@�C������������B
    FILE* output_file = nullptr;
    {
        char file_path[PATH_MAX];
        sprintf(file_path, "%s/shuffled.bin", shuffled_kifu_dir.c_str());
        output_file = std::fopen(file_path, "wb");

        if (std::setvbuf(output_file, nullptr, _IOFBF, std::numeric_limits<int>::max())) {
            sync_cout << "info string Failed to set the output buffer: output_file_path_="
                << file_path << sync_endl;
            return;
        }
    }

    // �e�t�@�C�����V���b�t������
    for (const auto& file_path : file_paths) {
        sync_cout << "info string " << file_path << sync_endl;

        // �t�@�C���S�̂�ǂݍ���
        FILE* input_file = std::fopen(file_path.c_str(), "rb");
        if (input_file == nullptr) {
            sync_cout << "info string Failed to open a kifu file. " << file_path << sync_endl;
            return;
        }

        if (std::setvbuf(input_file, nullptr, _IOFBF, std::numeric_limits<int>::max())) {
            sync_cout << "info string Failed to set the output buffer: input_file="
                << file_path << sync_endl;
            return;
        }

        fseeko(input_file, 0, SEEK_END);
        int64_t size = ftello(input_file);
        fseeko(input_file, 0, SEEK_SET);
        if (!paired_shuffle) {
            std::vector<PackedSfenValue> records(size / sizeof(PackedSfenValue));
            std::fread(&records[0], sizeof(PackedSfenValue), size / sizeof(PackedSfenValue), input_file);
            std::fclose(input_file);
            input_file = nullptr;

            // �����S�̂��V���b�t������
            std::shuffle(records.begin(), records.end(), mt);

            // �V���b�t���ς݃t�@�C�����폜����
            std::filesystem::remove(file_path);

            // �o�̓t�@�C���ɏ����o��
            if (std::fwrite(&records[0], sizeof(PackedSfenValue), records.size(), output_file) !=
                records.size()) {
                sync_cout << "info string Failed to write records to a kifu file. " << file_path
                    << sync_endl;
                return;
            }
        }
        else {
            if (size % sizeof(PairedPackedSfenValue) != 0) {
                sync_cout << "info string Unexpected paired temp file size: " << file_path << sync_endl;
                std::fclose(input_file);
                return;
            }
            std::vector<PairedPackedSfenValue> records(size / sizeof(PairedPackedSfenValue));
            std::fread(&records[0], sizeof(PairedPackedSfenValue), size / sizeof(PairedPackedSfenValue), input_file);
            std::fclose(input_file);
            input_file = nullptr;

            std::shuffle(records.begin(), records.end(), mt);
            std::filesystem::remove(file_path);

            if (std::fwrite(&records[0], sizeof(PairedPackedSfenValue), records.size(), output_file) !=
                records.size()) {
                sync_cout << "info string Failed to write paired records to a kifu file. " << file_path
                    << sync_endl;
                return;
            }
        }
    }

    std::fclose(output_file);
    output_file = nullptr;

    GlobalOptions = old_global_options;
}

#endif
