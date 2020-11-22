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
}

void Tanuki::AnalyzeProgress()
{
	auto kifu_folder_path = Options["KifuDir"];
	KifuReader reader(kifu_folder_path, 1);

	Progress progress;
	progress.Load();
	
	int count[kNumBars] = {};

	Learner::PackedSfenValue packed_sfen_value;
	auto& position = Threads[0]->rootPos;
	int num_positions = 0;
	while (reader.Read(packed_sfen_value)) {
		StateInfo state_info = {};
		position.set_from_packed_sfen(packed_sfen_value.sfen, &state_info, Threads[0]);
		double p = progress.Estimate(position);
		++count[static_cast<int>(p * kNumBars)];
		++num_positions;
	}

	double num_positions_adjusted = 0.0;
	for (int i = 0; i < kNumBars; ++i) {
		num_positions_adjusted += count[i] * (kNumBars - i) / static_cast<double>(kNumBars);
	}

	for (int i = 0; i < kNumBars; ++i) {
		printf("%f,%f,%f\n",
			i / static_cast<double>(kNumBars),
			count[i] / static_cast<double>(num_positions),
			count[i] * (kNumBars - i) / static_cast<double>(kNumBars) / num_positions_adjusted);
	}
}

#endif
