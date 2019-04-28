#ifndef _TANUKI_KIFU_WRITER_H_
#define _TANUKI_KIFU_WRITER_H_

#include "extra/config.h"

#ifdef EVAL_LEARN

#include "learn/learn.h"
#include "position.h"
#include "shogi.h"

namespace Tanuki {
	class KifuWriter {
	public:
		KifuWriter(const std::string& output_file_path);
		virtual ~KifuWriter();
		bool Write(const Learner::PackedSfenValue& record);
		bool Close();

	private:
		const std::string output_file_path_;
		FILE* file_ = nullptr;

		bool EnsureOpen();
	};
}

#endif

#endif
