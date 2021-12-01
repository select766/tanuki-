#include "denryu2_investigation.h"

#include <filesystem>

#include "tanuki_progress.h"
#include "position.h"
#include "thread.h"
#include "csa.h"

void Denryu2::ExtractTime()
{
	auto progress = std::make_unique<Tanuki::Progress>();
	if (!progress->Load()) {
		std::exit(1);
	}

	Position pos;

	for (const auto& directory_entry : std::filesystem::directory_iterator("C:\\shogi\\dr2_kifu_csa")) {
		const auto& path = directory_entry.path();
		const auto& filename = path.filename().string();

		Color toarubaito_color;
		if (filename.find("_toarubaito_") != std::string::npos) {
			toarubaito_color = BLACK;
		}
		else if (filename.find("_toarubaito-") != std::string::npos) {
			toarubaito_color = WHITE;
		}
		else {
			continue;
		}

		int day;
		if (filename.find("+dr2prdy1-") != std::string::npos) {
			day = 1;
		}
		else {
			day = 2;
		}

		std::vector<StateInfo> state_info(512);
		pos.set_hirate(&state_info[0], Threads.main());

		std::ifstream ifs(path);
		std::string line;
		while (std::getline(ifs, line) && line != "+")
			;

		Color color = BLACK;
		while (std::getline(ifs, line) && line.find("%") != 0) {
			auto csa_move_str = line.substr(1);
			std::string time_str;
			std::getline(ifs, time_str);
			auto time = std::atoi(time_str.substr(1).c_str());

			if (toarubaito_color == color) {
				auto p = progress->Estimate(pos);
				auto progress_type = static_cast<int>(p * 3.0);
				std::printf("%d,%d,%d\n", day, progress_type, time);
			}

			auto move = CSA::to_move(pos, csa_move_str);
			pos.do_move(move, state_info[pos.game_ply()]);

			color = ~color;
		}
	}

	std::cout << "done" << std::endl;
}
