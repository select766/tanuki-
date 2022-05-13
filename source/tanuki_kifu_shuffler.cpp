#pragma optimize( "", off )

#include "tanuki_kifu_shuffler.h"
#include "config.h"

#ifdef EVAL_LEARN

#include <cstdio>
#include <ctime>
#include <direct.h>
#include <omp.h>
#include <random>

#include "learn/learn.h"
#include "misc.h"
#include "tanuki_kifu_reader.h"
#include "tanuki_kifu_writer.h"
#include "thread.h"

using Learner::PackedSfenValue;

namespace {
    static const constexpr char* kShuffledKifuDir = "ShuffledKifuDir";
    static const constexpr char* kApplyQSearch = "ApplyQSearch";
    // シャッフル後のファイル数
    // Windowsでは一度に512個までのファイルしか開けないため
    // 256個に制限しておく
    static const constexpr int kNumShuffledKifuFiles = 256;
    static const constexpr int kMaxPackedSfenValues = 1024 * 1024;
}

void Tanuki::InitializeShuffler(USI::OptionsMap& o) {
    o[kShuffledKifuDir] << USI::Option("kifu_shuffled");
    o[kApplyQSearch] << USI::Option(false);
}

void Tanuki::ShuffleKifu() {
    GlobalOptions_ old_global_options = GlobalOptions;
    //GlobalOptions.use_eval_hash = false;
    //GlobalOptions.use_hash_probe = false;

    int num_threads = (int)Options["Threads"];
    omp_set_num_threads(num_threads);

    Search::LimitsType limits;
    // 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
    limits.max_game_ply = 1 << 16;
    limits.depth = MAX_PLY;
    limits.silent = true;
    limits.enteringKingRule = EKR_27_POINT;
    Search::Limits = limits;

    // 棋譜を入力し、複数のファイルにランダムに追加していく
    sync_cout << "info string Reading and dividing kifu files..." << sync_endl;

    std::string kifu_dir = Options["KifuDir"];
    std::string shuffled_kifu_dir = Options[kShuffledKifuDir];
    bool apply_qsearch = Options[kApplyQSearch];

    sync_cout << "kifu_dir=" << kifu_dir << sync_endl;
    sync_cout << "shuffled_kifu_dir=" << shuffled_kifu_dir << sync_endl;

    auto reader = std::make_unique<KifuReader>(kifu_dir, 1);
    _mkdir(shuffled_kifu_dir.c_str());

    std::vector<std::string> file_paths;
    for (int file_index = 0; file_index < kNumShuffledKifuFiles; ++file_index) {
        char file_path[_MAX_PATH];
        sprintf(file_path, "%s/shuffled.%03d.bin", shuffled_kifu_dir.c_str(), file_index);
        file_paths.push_back(file_path);
    }

    sync_cout << "info string Opening output files..." << sync_endl;
    std::vector<std::shared_ptr<KifuWriter> > writers;
    for (const auto& file_path : file_paths) {
        //sync_cout << "file_path=" << file_path << sync_endl;
        writers.push_back(std::make_shared<KifuWriter>(file_path));
    }

    sync_cout << "info string Starting dividing..." << sync_endl;

    std::mt19937_64 mt(std::time(nullptr));
    std::uniform_int_distribution<> dist(0, kNumShuffledKifuFiles - 1);
    int64_t num_records = 0;
    for (;;) {
        std::vector<PackedSfenValue> records;
        {
            PackedSfenValue record;
            while (static_cast<int>(records.size()) < kMaxPackedSfenValues && reader->Read(record)) {
                records.push_back(record);
            }
        }

        if (records.empty()) {
            break;
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

                    // Root局面と末端局面の手番が異なる場合、評価値と勝敗を反転する。
                    if (root_color != leaf_color) {
                        record.score = -record.score;
                        record.game_result = -record.game_result;
                    }
                }
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

    sync_cout << "info string Starting shuffling..." << sync_endl;

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

    GlobalOptions = old_global_options;
}

#endif
