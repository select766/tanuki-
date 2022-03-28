#ifndef _TANUKI_KIFU_READER_H_
#define _TANUKI_KIFU_READER_H_

#include "config.h"

#ifdef EVAL_LEARN

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "learn/learn.h"
#include "usi.h"

namespace Tanuki {
	class KifuReader {
	public:
		KifuReader(const std::string& folder_name, int num_loops, int num_threads = 1);
		virtual ~KifuReader();
		bool Read(Learner::PackedSfenValue& record, int thread_index = 0);
		bool Read(int num_records, std::vector<Learner::PackedSfenValue>& records, int thread_index = 0);
		bool Close();
		static void Initialize(USI::OptionsMap& o);

	private:
		const std::string folder_name_;
		const int num_loops_;
		std::vector<std::string> file_paths_;
		std::vector<FILE*> files_;
		// file_index_ >= file_paths_.size()となる場合もある。
		// int local_file_index = file_index_++ % file_paths_.size()等として使うこと。
		// file_index_ > file_paths_.size() * num_loops_が終了条件
		std::atomic<int> file_index_;
	};
}

#endif

#endif
