#include "tanuki_kifu_reader.h"
#include "config.h"

#ifdef EVAL_LEARN

#include <filesystem>
#include <numeric>
#include <sstream>

#include "misc.h"
#include "usi.h"

using USI::Option;
using USI::OptionsMap;
using Learner::PackedSfenValue;

namespace {
	constexpr int kBufferSize = 1024 * 1024;
}

Tanuki::KifuReader::KifuReader(const std::string& folder_name, int num_loops, int num_threads)
	: folder_name_(folder_name), num_loops_(num_loops), files_(num_threads), file_index_(0) {
	sync_cout << "info string Enumerating the files in " << folder_name << sync_endl;
	for (const auto& file : std::filesystem::directory_iterator(folder_name)) {
		sync_cout << file.path() << sync_endl;
		file_paths_.push_back(file.path().string());
	}
}

Tanuki::KifuReader::~KifuReader() { Close(); }

bool Tanuki::KifuReader::Read(int num_records, std::vector<PackedSfenValue>& records, int thread_index) {
	records.resize(num_records);
	for (auto& record : records) {
		if (!Read(record, thread_index)) {
			return false;
		}
	}
	return true;
}

bool Tanuki::KifuReader::Read(PackedSfenValue& record, int thread_index) {
	auto& file = files_[thread_index];

	// まだファイルを開いていない場合、ファイルを開く
	if (file == nullptr) {
		int local_file_index = file_index_++ % file_paths_.size();
		sync_cout << "info string local_file_index=" << local_file_index << " file_index_=" << file_index_ << sync_endl;
		sync_cout << "info string Opening " << file_paths_[local_file_index] << sync_endl;
		file = std::fopen(file_paths_[local_file_index].c_str(), "rb");

		if (!file) {
			sync_cout << "into string Failed to open a kifu file: " << file_paths_[local_file_index] << sync_endl;
			return false;
		}

		if (std::setvbuf(file, nullptr, _IOFBF, kBufferSize)) {
			sync_cout << "into string Failed to set a file buffer: " << file_paths_[local_file_index] << sync_endl;
		}
	}

	// すべてのファイルをnum_loops_回開いたら終了する
	if (file_index_ > static_cast<int>(file_paths_.size()) * num_loops_) {
		return false;
	}

	// 1局面読み込む
	if (std::fread(&record, sizeof(record), 1, file) == 1) {
		return true;
	}

	// 読み込みに失敗したらファイルを閉じ、次のファイルを開く準備をする
	if (std::fclose(file)) {
		sync_cout << "info string Failed to close a kifu file." << sync_endl;
		return false;
	}

	file = nullptr;

	// 次のファイルを開く
	int local_file_index = file_index_++ % file_paths_.size();

	// すべてのファイルをnum_loops_回開いたら終了する
	if (file_index_ > static_cast<int>(file_paths_.size()) * num_loops_) {
		return false;
	}

	sync_cout << "info string local_file_index=" << local_file_index << " file_index_=" << file_index_ << sync_endl;
	sync_cout << "info string Opening " << file_paths_[local_file_index] << sync_endl;
	file = std::fopen(file_paths_[local_file_index].c_str(), "rb");
	if (file == nullptr) {
		// ファイルを開くのに失敗したら
		// 読み込みを終了する
		sync_cout << "into string Failed to open a kifu file: " << file_paths_[local_file_index]
			<< sync_endl;
		return false;
	}

	if (std::setvbuf(file, nullptr, _IOFBF, kBufferSize)) {
		sync_cout << "into string Failed to set a file buffer: " << file_paths_[local_file_index]
			<< sync_endl;
	}

	if (std::fread(&record, sizeof(record), 1, file) != 1) {
		return false;
	}

	return true;
}

bool Tanuki::KifuReader::Close() {
	for (auto& file : files_) {
		if (file == nullptr) {
			continue;
		}

		if (fclose(file)) {
			return false;
		}

		file = nullptr;
	}

	return true;
}

#endif
