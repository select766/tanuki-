#include "tanuki_analysis.h"
#include "config.h"

#include "tanuki_kifu_reader.h"
#include "tanuki_progress.h"
#include "thread.h"
#include "usi.h"

#ifdef EVAL_LEARN

namespace
{
	const int kNumBars = 100;
	const int kValuePerBucket = 100;
	const int kNumPositions = 10000000;
}

void Tanuki::AnalyzeProgress()
{
	auto kifu_folder_path = Options["KifuDir"];
	KifuReader reader(kifu_folder_path, 1);

	Progress progress;
	if (!progress.Load()) {
		std::exit(1);
	}

	int count[kNumBars] = {};

	Learner::PackedSfenValue packed_sfen_value;
	auto& position = Threads[0]->rootPos;
	int num_positions = 0;
	for (; num_positions < kNumPositions && reader.Read(packed_sfen_value); ++num_positions) {
		StateInfo state_info = {};
		position.set_from_packed_sfen(packed_sfen_value.sfen, &state_info, Threads[0]);
		double p = progress.Estimate(position);
		int bucket_index = std::abs(packed_sfen_value.score) / kValuePerBucket;
		if (bucket_index >= kNumBars) {
			continue;
		}
		++count[bucket_index];
		++num_positions;
	}

	//for (int i = 0; i < kNumBars; ++i) {
	//	printf("%f\n", count[i] / static_cast<double>(num_positions));
	//}

	for (int i = 0; i < kNumBars; ++i) {
		printf("%f\n", count[i] / static_cast<double>(num_positions));
	}
}

#endif
