#include "kifu_reader.h"

#include <numeric>
#include <sstream>
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX

#include "misc.h"
#include "shogi.h"

using USI::Option;
using USI::OptionsMap;

namespace {
	constexpr Value kCloseOutValueThreshold = Value(VALUE_INFINITE);
	constexpr int kBufferSize = 1 << 24; // 16MB
	constexpr char* kOptionValueReadBatchSize = "ReadBatchSize";
}

Learner::KifuReader::KifuReader(const std::string& folder_name, bool shuffle)
	: folder_name_(folder_name), shuffle_(shuffle),
	read_match_size_((int)Options[kOptionValueReadBatchSize]) {
}

Learner::KifuReader::~KifuReader() {
	Close();
}

void Learner::KifuReader::Initialize(USI::OptionsMap& o) {
	o[kOptionValueReadBatchSize] << Option(1000'0000, 0, INT_MAX);
}

bool Learner::KifuReader::Read(int num_records, std::vector<Record>& records) {
	records.resize(num_records);
	for (auto& record : records) {
		if (!Read(record)) {
			return false;
		}
	}
	return true;
}

bool Learner::KifuReader::Read(Record& record) {
	if (!EnsureOpen()) {
		return false;
	}

	if (record_index_ >= static_cast<int>(records_.size())) {
		records_.clear();
		while (records_.size() < read_match_size_) {
			if (std::fread(&record, sizeof(record), 1, file_) != 1) {
				// �t�@�C���̏I�[�ɒH�蒅����
				if (++file_index_ >= static_cast<int>(file_paths_.size())) {
					// �t�@�C�����X�g�̏I�[�ɂ��ǂ蒅����
					if (shuffle_) {
						// �t�@�C�����X�g���V���b�t������
						std::shuffle(file_paths_.begin(), file_paths_.end(), std::mt19937_64(std::random_device()()));
					}

					// �t�@�C���C���f�N�X�����Z�b�g����
					file_index_ = 0;
				}

				// ���̃t�@�C�����J��
				if (fclose(file_)) {
					sync_cout << "info string Failed to close a kifu file." << sync_endl;
				}

				file_ = std::fopen(file_paths_[file_index_].c_str(), "rb");
				if (!file_) {
					// �t�@�C�����J���̂Ɏ��s������
					// �ǂݍ��݂��I������
					sync_cout << "into string Failed to open a kifu file: "
						<< file_paths_[file_index_] << sync_endl;
					return false;
				}

				if (std::setvbuf(file_, nullptr, _IOFBF, kBufferSize)) {
					sync_cout << "into string Failed to set a file buffer: "
						<< file_paths_[file_index_] << sync_endl;
				}

				if (std::fread(&record, sizeof(record), 1, file_) != 1) {
					// ��̃t�@�C���̏ꍇ�͂����ɗ���
					// �Ƃ肠����continue���Ă��̃t�@�C��������
					continue;
				}
			}

			if (abs(record.value) > kCloseOutValueThreshold) {
				// �]���l�̐�Βl���傫������ꍇ�͎g�p���Ȃ�
				continue;
			}

			records_.push_back(record);
		}

		record_index_ = 0;
	}

	if (records_.empty()) {
		return false;
	}

	// �ǂݍ��񂾋ǖʐ���kBatchSize�ɖ����Ȃ��ꍇ�A
	// permutation�z��o�R�̃A�N�Z�X�Ŕ͈͊O�A�N�Z�X���N����B
	// �����h�����߁Apermutation�z����č쐬����B
	if (records_.size() != permutation_.size()) {
		permutation_.resize(records_.size());
		std::iota(permutation_.begin(), permutation_.end(), 0);
		if (shuffle_) {
			std::shuffle(permutation_.begin(), permutation_.end(), std::mt19937_64(std::time(nullptr)));
		}

	}

	record = records_[permutation_[record_index_]];
	++record_index_;

	return true;
}

bool Learner::KifuReader::Close() {
	if (fclose(file_)) {
		return false;
	}
	file_ = nullptr;
	return true;
}

bool Learner::KifuReader::EnsureOpen() {
	if (!file_paths_.empty()) {
		return true;
	}

	// http://qiita.com/tkymx/items/f9190c16be84d4a48f8a
	HANDLE find = nullptr;
	WIN32_FIND_DATAA find_data = { 0 };

	std::string search_name = folder_name_ + "/*.*";
	find = FindFirstFileA(search_name.c_str(), &find_data);

	if (find == INVALID_HANDLE_VALUE) {
		sync_cout << "Failed to find kifu files." << sync_endl;
		sync_cout << "search_name=" << search_name << sync_endl;
		return false;
	}

	do {
		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			continue;
		}

		std::string file_path = folder_name_ + "/" + find_data.cFileName;
		file_paths_.push_back(file_path);
	} while (FindNextFileA(find, &find_data));

	FindClose(find);
	find = nullptr;

	if (file_paths_.empty()) {
		return false;
	}

	std::random_shuffle(file_paths_.begin(), file_paths_.end());

	file_ = std::fopen(file_paths_[0].c_str(), "rb");

	if (!file_) {
		sync_cout << "into string Failed to open a kifu file: " << file_paths_[0]
			<< sync_endl;
		return false;
	}

	if (std::setvbuf(file_, nullptr, _IOFBF, kBufferSize)) {
		sync_cout << "into string Failed to set a file buffer: "
			<< file_paths_[0] << sync_endl;
	}

	return true;
}