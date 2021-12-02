#include "denryu2_investigation.h"

#include <filesystem>
#include <random>

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

void Denryu2::ExtractGames()
{
	// 棋譜を読み込む
	sync_cout << "Reading records..." << sync_endl;
	std::string book_file = "C:\\shogi\\floodgate\\wdoor.sfen";
	std::ifstream ifs(book_file);
	if (!ifs) {
		sync_cout << "info string Failed to read the progress book file." << sync_endl;
		std::exit(-1);
	}

	std::vector<std::string> valid_games;
	std::string line;
	int index = 0;
	while (std::getline(ifs, line)) {
		std::istringstream iss(line);
		std::string move_str;
		Position pos;
		std::vector<StateInfo> state_info(512);
		pos.set_hirate(&state_info[0], Threads.main());
		while (iss >> move_str) {
			if (move_str == "startpos" || move_str == "moves") {
				continue;
			}

			Move move = USI::to_move(pos, move_str);
			if (!is_ok(move) || !pos.legal(move)) {
				break;
			}

			pos.do_move(move, state_info[pos.game_ply()]);
		}

		if (pos.is_mated() || pos.DeclarationWin() != MOVE_NONE) {
			valid_games.push_back(line);
		}

		if (++index % 10000 == 0) {
			sync_cout << index << sync_endl;
		}
	}

	sync_cout << "num_records: " << valid_games.size() << sync_endl;
	std::random_device rd;
	std::mt19937_64 mt(rd());
	std::shuffle(valid_games.begin(), valid_games.end(), mt);

	for (int game_index = 0; game_index < 10; ++game_index) {
		sync_cout << valid_games[game_index] << sync_endl;
	}
}
