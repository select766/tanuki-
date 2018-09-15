#include "tanuki_kifu_writer.h"

#include "misc.h"

namespace {
	constexpr int kBufferSize = 1 << 25;  // 32MB
}

Tanuki::KifuWriter::KifuWriter(const std::string& output_file_path)
	: output_file_path_(output_file_path) {}

Tanuki::KifuWriter::~KifuWriter() { Close(); }

bool Tanuki::KifuWriter::Write(const Learner::PackedSfenValue& record) {
	if (!EnsureOpen()) {
		return false;
	}

	if (std::fwrite(&record, sizeof(record), 1, file_) != 1) {
		return false;
	}

	return true;
}

bool Tanuki::KifuWriter::Close() {
	if (!file_) {
		return true;
	}

	bool result = true;
	if (std::fclose(file_) != 0) {
		sync_cout << "info string Failed to close the output kifu file: output_file_path="
			<< output_file_path_ << sync_endl;
		result = false;
	}
	file_ = nullptr;
	return result;
}

bool Tanuki::KifuWriter::EnsureOpen() {
	if (file_) {
		return true;
	}
	file_ = std::fopen(output_file_path_.c_str(), "wb");
	if (!file_) {
		sync_cout << "info string Failed to open the output kifu file: output_file_path_="
			<< output_file_path_ << sync_endl;
		return false;
	}

	if (std::setvbuf(file_, nullptr, _IOFBF, kBufferSize)) {
		sync_cout << "info string Failed to set the output buffer: output_file_path_="
			<< output_file_path_ << sync_endl;
		return false;
	}

	return true;
}
