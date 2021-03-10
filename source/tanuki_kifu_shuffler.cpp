#include "tanuki_kifu_shuffler.h"
#include "config.h"

#ifdef EVAL_LEARN

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <random>

#include "tanuki_kifu_reader.h"
#include "tanuki_kifu_writer.h"
#include "misc.h"

using Learner::PackedSfenValue;

namespace {
	static const constexpr char* kShuffledKifuDir = "ShuffledKifuDir";
	// シャッフル後のファイル数
	// Windowsでは一度に512個までのファイルしか開けないため
	// 256個に制限しておく
	static const constexpr int kNumShuffledKifuFiles = 256;
	static const constexpr int kMaxPackedSfenValues = 1024 * 1024;
}

void Tanuki::InitializeShuffler(USI::OptionsMap& o) {
	o[kShuffledKifuDir] << USI::Option("kifu_shuffled");
}

void Tanuki::ShuffleKifu() {
	// 棋譜を入力し、複数のファイルにランダムに追加していく
	sync_cout << "info string Reading and dividing kifu files..." << sync_endl;

	std::string kifu_dir = Options["KifuDir"];
	std::string shuffled_kifu_dir = Options[kShuffledKifuDir];

	auto reader = std::make_unique<KifuReader>(kifu_dir, 1);
	std::filesystem::create_directories(shuffled_kifu_dir);

	std::vector<std::string> file_paths;
	for (int file_index = 0; file_index < kNumShuffledKifuFiles; ++file_index) {
		char file_path[1024];
		sprintf(file_path, "%s/shuffled.%03d.bin", shuffled_kifu_dir.c_str(), file_index);
		file_paths.push_back(file_path);
	}

	std::vector<std::shared_ptr<KifuWriter> > writers;
	for (const auto& file_path : file_paths) {
		writers.push_back(std::make_shared<KifuWriter>(file_path));
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

#endif
