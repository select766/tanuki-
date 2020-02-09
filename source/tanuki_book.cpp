//#pragma optimize( "", off )

#include "tanuki_book.h"
#include "config.h"

#ifdef EVAL_LEARN

#include <atomic>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <queue>
#include <random>
#include <set>
#include <sstream>

#include "evaluate.h"
#include "extra/book/book.h"
#include "learn/learn.h"
#include "misc.h"
#include "position.h"
#include "tanuki_progress_report.h"
#include "thread.h"

using Book::BookMoveSelector;
using Book::BookPos;
using Book::MemoryBook;
using USI::Option;

namespace {
	constexpr const char* kBookSfenFile = "BookSfenFile";
	constexpr const char* kBookMaxMoves = "BookMaxMoves";
	constexpr const char* kBookFile = "BookFile";
	constexpr const char* kBookSearchDepth = "BookSearchDepth";
	constexpr const char* kBookSearchNodes = "BookSearchNodes";
	constexpr const char* kBookInputFile = "BookInputFile";
	constexpr const char* kBookOutputFile = "BookOutputFile";
	constexpr const char* kThreads = "Threads";
	constexpr const char* kMultiPV = "MultiPV";
	constexpr const char* kBookOverwriteExistingPositions = "OverwriteExistingPositions";
	constexpr const char* kBookNarrowBook = "NarrowBook";
	constexpr int kShowProgressPerAtMostSec = 1 * 60 * 60;	// 1時間
	constexpr time_t kSavePerAtMostSec = 6 * 60 * 60;		// 6時間

	struct SfenAndMove {
		std::string sfen;
		Move best_move;
		Move next_move;
	};
}

bool Tanuki::InitializeBook(USI::OptionsMap& o) {
	o[kBookSfenFile] << Option("merged.sfen");
	o[kBookMaxMoves] << Option(32, 0, 256);
	o[kBookSearchDepth] << Option(64, 0, 256);
	o[kBookSearchNodes] << Option(500000 * 60, 0, INT_MAX);
	o[kBookInputFile] << Option("user_book1.db");
	o[kBookOutputFile] << Option("user_book2.db");
	o[kBookOverwriteExistingPositions] << Option(false);
	return true;
}

// sfen棋譜ファイルから定跡データベースを作成する
bool Tanuki::CreateRawBook() {
	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	std::string sfen_file = Options[kBookSfenFile];
	int max_moves = (int)Options[kBookMaxMoves];
	bool narrow_book = static_cast<bool>(Options[kBookNarrowBook]);

	std::ifstream ifs(sfen_file);
	if (!ifs) {
		sync_cout << "info string Failed to open the sfen file." << sync_endl;
		std::exit(-1);
	}
	std::string line;

	MemoryBook memory_book;
	StateInfo state_info[4096] = {};
	StateInfo* state = state_info + 8;
	int num_records = 0;
	while (std::getline(ifs, line)) {
		std::istringstream iss(line);
		Position pos;
		pos.set_hirate(state, Threads[0]);

		std::string token;
		int num_moves = 0;
		while (num_moves < max_moves && iss >> token) {
			if (token == "startpos" || token == "moves") {
				continue;
			}
			Move move = USI::to_move(pos, token);
			if (!pos.pseudo_legal(move) || !pos.legal(move)) {
				continue;
			}

			BookPos book_pos(move, Move::MOVE_NONE, 0, 0, 1);
			memory_book.insert(pos.sfen(), book_pos);

			pos.do_move(move, state[num_moves]);
			++num_moves;
		}

		++num_records;
		if (num_records % 10000 == 0) {
			sync_cout << "info string " << num_records << sync_endl;
		}
	}

	std::string book_file = Options[kBookFile];
	book_file = "book/" + book_file;
	memory_book.write_book(book_file);

	return true;
}

bool Tanuki::CreateScoredBook() {
	int num_threads = (int)Options[kThreads];
	std::string input_book_file = Options[kBookInputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];
	int multi_pv = (int)Options[kMultiPV];
	std::string output_book_file = Options[kBookOutputFile];
	bool overwrite_existing_positions = static_cast<bool>(Options[kBookOverwriteExistingPositions]);

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string multi_pv=" << multi_pv << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;
	sync_cout << "info string overwrite_existing_positions=" << overwrite_existing_positions
		<< sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	MemoryBook input_book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	input_book.read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book|=" << input_book.book_body.size() << sync_endl;

	MemoryBook output_book;
	output_book_file = "book/" + output_book_file;
	sync_cout << "Reading output book file: " << output_book_file << sync_endl;
	output_book.read_book(output_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.book_body.size() << sync_endl;

	std::vector<std::string> sfens;
	for (const auto& sfen_and_count : input_book.book_body) {
		if (!overwrite_existing_positions &&
			output_book.book_body.find(sfen_and_count.first) != output_book.book_body.end()) {
			continue;
		}
		sfens.push_back(sfen_and_count.first);
	}
	int num_sfens = sfens.size();
	sync_cout << "Number of the positions to be processed: " << num_sfens << sync_endl;

	time_t start_time = 0;
	std::time(&start_time);

	std::atomic_int global_position_index;
	global_position_index = 0;
	std::mutex output_book_mutex;
	ProgressReport progress_report(num_sfens, kShowProgressPerAtMostSec);
	time_t last_save_time_sec = std::time(nullptr);

	std::vector<std::thread> threads;
	std::atomic_int global_pos_index;
	global_pos_index = 0;
	std::atomic_int global_num_processed_positions;
	global_num_processed_positions = 0;
	for (int thread_index = 0; thread_index < num_threads; ++thread_index) {
		auto procedure = [thread_index, num_sfens, search_depth, multi_pv,
			search_nodes, &global_pos_index, &sfens, &output_book, &output_book_mutex, &progress_report,
			&output_book_file, &global_num_processed_positions, &last_save_time_sec]() {
			WinProcGroup::bindThisThread(thread_index);

			for (int position_index = global_pos_index++; position_index < num_sfens;
				position_index = global_pos_index++) {
				const std::string& sfen = sfens[position_index];
				Thread& thread = *Threads[thread_index];
				StateInfo state_info = {};
				Position& pos = thread.rootPos;
				pos.set(sfen, &state_info, &thread);

				if (pos.is_mated()) {
					continue;
				}

				Learner::search(pos, search_depth, multi_pv, search_nodes);

				int num_pv = std::min(multi_pv, static_cast<int>(thread.rootMoves.size()));
				for (int pv_index = 0; pv_index < num_pv; ++pv_index) {
					const auto& root_move = thread.rootMoves[pv_index];
					Move best = Move::MOVE_NONE;
					if (root_move.pv.size() >= 1) {
						best = root_move.pv[0];
					}
					Move next = Move::MOVE_NONE;
					if (root_move.pv.size() >= 2) {
						next = root_move.pv[1];
					}
					int value = root_move.score;
					BookPos book_pos(best, next, value, search_depth, 0);
					{
						std::lock_guard<std::mutex> lock(output_book_mutex);
						output_book.insert(sfen, book_pos);
					}
				}

				// 進捗状況を表示する
				int num_processed_positions = ++global_num_processed_positions;
				progress_report.Show(num_processed_positions);

				// 一定時間ごとに保存する
				{
					std::lock_guard<std::mutex> lock(output_book_mutex);
					if (last_save_time_sec + kSavePerAtMostSec < std::time(nullptr)) {
						output_book.write_book(output_book_file);
						last_save_time_sec = std::time(nullptr);
					}
				}
			}
		};
		threads.push_back(std::thread(procedure));
	}

	for (auto& thread : threads) {
		thread.join();
	}

	output_book.write_book(output_book_file);

	return true;
}

bool Tanuki::ExtendBook() {
	int num_threads = (int)Options[kThreads];
	std::string input_file = Options[kBookInputFile];
	std::string output_file = Options[kBookOutputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];

	// スレッド0版を未展開のノードを探すために、
	// それ以外のスレッドを未展開のノードの指し手を決定するために使うため
	// 2スレッド以上必要となる。
	ASSERT_LV3(num_threads >= 2);

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string input_file=" << input_file << sync_endl;
	sync_cout << "info string output_file=" << output_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	BookMoveSelector input_book;
	sync_cout << "Reading input book file: " << input_file << sync_endl;
	input_book.GetMemoryBook().read_book("book/" + input_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book|=" << input_book.GetMemoryBook().book_body.size() << sync_endl;

	BookMoveSelector output_book;
	sync_cout << "Reading output book file: " << output_file << sync_endl;
	output_book.GetMemoryBook().read_book("book/" + output_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.GetMemoryBook().book_body.size() << sync_endl;

	time_t last_save_time_sec = std::time(nullptr);

	std::mutex sfens_mutex;
	std::mutex output_mutex;
	std::condition_variable cv;

	std::vector<std::thread> threads;
	std::set<std::string> sfens_to_be_processed;
	std::set<std::string> sfens_processing;
	// スレッド0番は未展開のノードを探すために使う
	// スレッド1番以降を未展開のノードの指し手を決定するために使う
	for (int thread_index = 1; thread_index < num_threads; ++thread_index) {
		auto procedure = [thread_index, search_depth, output_file, &last_save_time_sec,
			&sfens_to_be_processed, &sfens_processing, &input_book, &output_book, &sfens_mutex,
			&output_mutex, &cv]() {
			WinProcGroup::bindThisThread(thread_index);

			for (;;) {
				std::string sfen;
				{
					std::unique_lock<std::mutex> lock(sfens_mutex);
					cv.wait(lock,
						[&sfens_to_be_processed] { return !sfens_to_be_processed.empty(); });
					sfen = *sfens_to_be_processed.begin();
					sfens_to_be_processed.erase(sfens_to_be_processed.begin());
					sfens_processing.insert(sfen);
				}

				Position& pos = Threads[thread_index]->rootPos;
				StateInfo state_info[512];
				pos.set(sfen, &state_info[0], Threads[thread_index]);

				if (pos.is_mated()) {
					std::lock_guard<std::mutex> lock_sfens(sfens_mutex);
					sfens_processing.erase(sfen);

					std::lock_guard<std::mutex> lock_output(output_mutex);
					output_book.GetMemoryBook().insert(
						sfen, { Move::MOVE_RESIGN, Move::MOVE_NONE, -VALUE_MATE, search_depth, 1 });
				}

				auto value_and_pv = Learner::search(pos, search_depth);

				std::lock_guard<std::mutex> lock_sfens(sfens_mutex);
				sfens_processing.erase(sfen);

				std::lock_guard<std::mutex> lock_output(output_mutex);
				Move best_move =
					value_and_pv.second.empty() ? Move::MOVE_NONE : value_and_pv.second[0];
				Move next_move =
					value_and_pv.second.size() < 2 ? Move::MOVE_NONE : value_and_pv.second[1];
				output_book.GetMemoryBook().insert(
					sfen, Book::BookPos(best_move, next_move, value_and_pv.first, search_depth, 1));

				sync_cout << "|output_book|=" << output_book.GetMemoryBook().book_body.size()
					<< " |sfens_to_be_processed|=" << sfens_to_be_processed.size()
					<< " |sfens_processing|=" << sfens_processing.size() << sync_endl;

				if (last_save_time_sec + kSavePerAtMostSec < std::time(nullptr)) {
					continue;
				}

				// 上書き保存する
				output_book.GetMemoryBook().write_book("book/" + output_file);
				last_save_time_sec = std::time(nullptr);
			}
		};
		threads.push_back(std::thread(procedure));
	}

	bool input_is_first = true;
	for (;;) {
		Position& pos = Threads[0]->rootPos;
		StateInfo state_info[512] = {};
		pos.set_hirate(&state_info[0], Threads[0]);
		bool input_is_turn = input_is_first;

		for (;;) {
			if (input_is_turn) {
				input_is_turn = !input_is_turn;
				Move move = input_book.probe(pos);
				if (move != Move::MOVE_NONE && pos.pseudo_legal(move) && pos.legal(move)) {
					pos.do_move(move, state_info[pos.game_ply()]);
					continue;
				}

				// 入力側の定跡が外れたので終了する
				break;
			}
			else {
				input_is_turn = !input_is_turn;
				Move move = output_book.probe(pos);
				if (move != Move::MOVE_NONE && pos.pseudo_legal(move) && pos.legal(move)) {
					pos.do_move(move, state_info[pos.game_ply()]);
					continue;
				}

				std::string sfen = pos.sfen();
				std::lock_guard<std::mutex> lock(sfens_mutex);
				if (sfens_to_be_processed.find(sfen) != sfens_to_be_processed.end()) {
					// キューに入っている場合もスキップする
					// sync_cout << "in queue" << sync_endl;
					break;
				}

				if (sfens_processing.find(sfen) != sfens_processing.end()) {
					// 処理中の場合はスキップする
					// sync_cout << "in processing" << sync_endl;
					break;
				}

				// キューに入れる
				sync_cout << sfen << sync_endl;
				sfens_to_be_processed.insert(sfen);
				cv.notify_all();
				break;
			}
		}

		input_is_first = !input_is_first;
	}

	return true;
}

// 複数の定跡をマージする
// BookInputFileには「;」区切りで定跡データベースの古パースを指定する
// BookOutputFileにはbook以下のファイル名を指定する
bool Tanuki::MergeBook() {
	std::string input_file_list = Options[kBookInputFile];
	std::string output_file = Options[kBookOutputFile];

	sync_cout << "info string input_file_list=" << input_file_list << sync_endl;
	sync_cout << "info string output_file=" << output_file << sync_endl;

	BookMoveSelector output_book;
	sync_cout << "Reading output book file: " << output_file << sync_endl;
	output_book.GetMemoryBook().read_book("book/" + output_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.GetMemoryBook().book_body.size() << sync_endl;

	std::vector<std::string> input_files;
	{
		std::istringstream iss(input_file_list);
		std::string input_file;
		while (std::getline(iss, input_file, ';')) {
			input_files.push_back(input_file);
		}
	}

	for (int input_file_index = 0; input_file_index < input_files.size(); ++input_file_index) {
		sync_cout << (input_file_index + 1) << " / " << input_files.size() << sync_endl;

		const auto& input_file = input_files[input_file_index];
		BookMoveSelector input_book;
		sync_cout << "Reading input book file: " << input_file << sync_endl;
		input_book.GetMemoryBook().read_book(input_file);
		sync_cout << "done..." << sync_endl;
		sync_cout << "|input_book|=" << input_book.GetMemoryBook().book_body.size() << sync_endl;

		for (const auto& book_type : input_book.GetMemoryBook().book_body) {
			const auto& sfen = book_type.first;
			const auto& pos_move_list = book_type.second;

			uint64_t max_num = 0;
			for (const auto& pos_move : *pos_move_list) {
				max_num = std::max(max_num, pos_move.num);
			}

			for (const auto& pos_move : *pos_move_list) {
				if (max_num > 0 && pos_move.num == 0) {
					// 採択回数が設定されており、この手の採択回数が0の場合、
					// 手動でこの手を指さないよう調整されている。
					// そのような手はスキップする。
					continue;
				}
				output_book.GetMemoryBook().insert(sfen, pos_move, false);
			}
		}
	}

	output_book.GetMemoryBook().write_book("book/" + output_file);

	return true;
}

// 定跡の各指し手に評価値を付ける
// 相手の応手が設定されていない場合、読み筋から設定する。
bool Tanuki::SetScoreToMove() {
	int num_threads = (int)Options[kThreads];
	std::string input_book_file = Options[kBookInputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];
	std::string output_book_file = Options[kBookOutputFile];

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	MemoryBook input_book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	input_book.read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book|=" << input_book.book_body.size() << sync_endl;

	MemoryBook output_book;
	output_book_file = "book/" + output_book_file;
	sync_cout << "Reading output book file: " << output_book_file << sync_endl;
	output_book.read_book(output_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|output_book|=" << output_book.book_body.size() << sync_endl;

	std::vector<SfenAndMove> sfen_and_moves;

	// 未処理の局面をキューに登録する
	for (const auto& input_sfen_and_pos_move_list : input_book.book_body) {
		const auto& input_sfen = input_sfen_and_pos_move_list.first;

		auto output_book_moves_it = output_book.book_body.find(input_sfen);
		if (output_book_moves_it == output_book.book_body.end()) {
			// 出力定跡データベースに局面が登録されていなかった場合
			for (const auto& input_book_move : *input_sfen_and_pos_move_list.second) {
				sfen_and_moves.push_back({ input_sfen, input_book_move.bestMove,
					input_book_move.nextMove });
			}
			continue;
		}

		const auto& output_book_moves = *output_book_moves_it->second;
		for (const auto& input_book_move : *input_sfen_and_pos_move_list.second) {
			if (std::find_if(output_book_moves.begin(), output_book_moves.end(),
				[&input_book_move](const auto& x) {
					return USI::move(input_book_move.bestMove) == USI::move(x.bestMove);
				}) != output_book_moves.end()) {
				continue;
			}

			// 出力定跡データベースに指し手が登録されていなかった場合
			sfen_and_moves.push_back({ input_sfen, input_book_move.bestMove,
				input_book_move.nextMove });
		}
	}
	int num_sfen_and_moves = sfen_and_moves.size();
	sync_cout << "Number of the moves to be processed: " << num_sfen_and_moves << sync_endl;

	// マルチスレッド処理準備のため、インデックスを初期化する
	std::atomic_int global_sfen_and_move_index;
	global_sfen_and_move_index = 0;
	std::mutex output_book_mutex;

	// 進捗状況表示の準備
	ProgressReport progress_report(num_sfen_and_moves, kShowProgressPerAtMostSec);

	// 定跡を定期的に保存するための変数
	time_t last_save_time_sec = std::time(nullptr);

	// Apery式マルチスレッド処理
	std::vector<std::thread> threads;
	std::atomic_int global_num_processed_positions;
	global_num_processed_positions = 0;
	for (int thread_index = 0; thread_index < num_threads; ++thread_index) {
		auto procedure = [thread_index, num_sfen_and_moves, search_depth, search_nodes,
			&global_sfen_and_move_index, &sfen_and_moves, &output_book, &output_book_mutex,
			&progress_report, &output_book_file, &global_num_processed_positions, &input_book,
			&last_save_time_sec]() {
			WinProcGroup::bindThisThread(thread_index);

			for (int position_index = global_sfen_and_move_index++; position_index < num_sfen_and_moves;
				position_index = global_sfen_and_move_index++) {
				const auto& sfen_and_move = sfen_and_moves[position_index];
				const auto& sfen = sfen_and_move.sfen;
				Thread& thread = *Threads[thread_index];
				StateInfo state_info = {};
				Position& pos = thread.rootPos;
				pos.set(sfen, &state_info, &thread);
				Move best_move = pos.move16_to_move(sfen_and_move.best_move);
				Move next_move = MOVE_NONE;

				if (pos.is_mated()) {
					continue;
				}

				if (!pos.pseudo_legal(best_move) || !pos.legal(best_move)) {
					sync_cout << "Illegal move. sfen=" << sfen << " best_move=" <<
						USI::move(best_move) << " next_move=" << USI::move(next_move) << sync_endl;
					continue;
				}

				StateInfo state_info0;
				pos.do_move(best_move, state_info0);
				Eval::evaluate_with_no_return(pos);

				// この局面について探索する
				auto value_and_pv = Learner::search(pos, search_depth, 1, search_nodes);

				// ひとつ前の局面から見た評価値を代入する必要があるので、符号を反転する。
				Value value = -value_and_pv.first;

				auto pv = value_and_pv.second;
				if (next_move == MOVE_NONE && pv.size() >= 1) {
					// 定跡の手を指した次の局面なので、nextMoveにはpv[0]を代入する。
					// ただし、もともとnextMoveが設定されている場合、それを優先する。
					next_move = pv[0];
				}

				Depth depth = pos.this_thread()->completedDepth;

				// 指し手を出力先の定跡に登録する
				{
					std::lock_guard<std::mutex> lock(output_book_mutex);
					output_book.insert(sfen, BookPos(best_move, next_move, value, depth, 1));

					// 進捗状況を表示する
					int num_processed_positions = ++global_num_processed_positions;
					progress_report.Show(num_processed_positions);

					// 定跡をストレージに書き出す。
					if (last_save_time_sec + kSavePerAtMostSec < std::time(nullptr)) {
						output_book.write_book(output_book_file);
						last_save_time_sec = std::time(nullptr);
					}
				}
			}
		};
		threads.push_back(std::thread(procedure));
	}

	for (auto& thread : threads) {
		thread.join();
	}

	output_book.write_book(output_book_file);

	return true;
}

namespace {
	struct ValueMoveDepth {
		// 評価値
		Value value = VALUE_NONE;
		// この局面における指し手
		// 定跡の指し手に対する応手を定跡データベースに登録するため、指し手も返せるようにしておく
		Move move = MOVE_NONE;
		// 探索深さ
		Depth depth = DEPTH_NONE;
	};

	// (sfen, ValueAndDepth)
	// やねうら王本家では手数の部分を削除しているが、
	// 実装を容易にするため手数も含めて格納する。
	// 本来であればこれによる副作用を熟考する必要があるが、
	// よく分からなかったので適当に実装する。
	std::unordered_map<std::string, ValueMoveDepth> memo;

	// IgnoreBookPlyオプションがtrueの場合、
	// 与えられたsfen文字列からplyを取り除く
	std::string RemovePlyIfIgnoreBookPly(const std::string& sfen) {
		if (Options["IgnoreBookPly"]) {
			return StringExtension::trim_number(sfen);
		}
		else {
			return sfen;
		}
	}

	Move To16Bit(Move move) {
		return static_cast<Move>(static_cast<int>(move) & 0xffff);
	}

	// Nega-Max法で末端局面の評価値をroot局面に向けて伝搬する
	// book 定跡データベース
	// pos 現在の局面
	// value_and_depth_without_nega_max_parent SetScoreToMove()で設定した評価値と探索深さ
	//                                         現局面から見た評価値になるよう、符号を反転してから渡すこと
	ValueMoveDepth NegaMax(MemoryBook& book, Position& pos, const ValueMoveDepth vmd_without_nega_max_parent, int& counter) {
		if (++counter % 1000000 == 0) {
			sync_cout << counter << " |memo|=" << memo.size() << sync_endl;
		}

		// この局面に対してのキーとして使用するsfen文字列。
		// 必要に応じて末尾のplyを取り除いておく。
		std::string sfen = RemovePlyIfIgnoreBookPly(pos.sfen());
		auto book_moves = book.book_body.find(sfen);

		if (book_moves == book.book_body.end()) {
			// この局面が定跡データベースに登録されていない=末端局面である。
			// SetScoreToMove()による探索の結果を返す。
			return vmd_without_nega_max_parent;
		}

		auto& vmd = memo[sfen];
		if (vmd.depth != DEPTH_NONE) {
			// キャッシュにヒットした場合、その値を返す。
			return vmd;
		}

		if (pos.is_mated()) {
			// 詰んでいる場合
			vmd.value = mated_in(0);
			vmd.depth = DEPTH_ZERO;
			return vmd;
		}

		if (pos.DeclarationWin() != MOVE_NONE) {
			// 宣言勝ちできる場合
			vmd.value = mate_in(1);
			vmd.depth = DEPTH_ZERO;
			return vmd;
		}

		auto repetition_state = pos.is_repetition();
		switch (repetition_state) {
		case REPETITION_WIN:
			// 連続王手の千日手により価値の場合
			vmd.value = mate_in(MAX_PLY);
			vmd.depth = DEPTH_ZERO;
			return vmd;

		case REPETITION_LOSE:
			// 連続王手の千日手により負けの場合
			vmd.value = mated_in(MAX_PLY);
			vmd.depth = DEPTH_ZERO;
			return vmd;

		case REPETITION_DRAW:
			// 引き分けの場合
			vmd.value = static_cast<Value>(Options["Contempt"] * Eval::PawnValue / 100);
			vmd.depth = DEPTH_ZERO;
			return vmd;

		case REPETITION_SUPERIOR:
			// 優等局面の場合
			vmd.value = VALUE_SUPERIOR;
			vmd.depth = DEPTH_ZERO;
			return vmd;

		case REPETITION_INFERIOR:
			// 劣等局面の場合
			vmd.value = -VALUE_SUPERIOR;
			vmd.depth = DEPTH_ZERO;
			return vmd;

		default:
			break;
		}

		vmd.value = mated_in(0);
		vmd.depth = DEPTH_ZERO;
		// 全合法手について調べる
		for (const auto& move : MoveList<LEGAL_ALL>(pos)) {
			auto book_move = std::find_if(book_moves->second->begin(), book_moves->second->end(),
				[&move, &pos](const BookPos& book_move) {
					// 定跡データベースに含まれているmoveは16ビットのため、32ビットに変換する。
					Move move32 = pos.move16_to_move(book_move.bestMove);
					return move.move == move32;
				});

			// 定跡データベースの指し手に登録されている評価値と探索深さを
			// 子局面に渡すためのインスタンス
			ValueMoveDepth vmd_without_nega_max_child;
			if (book_move != book_moves->second->end()) {
				// 指し手につけられている評価値は現局面から見た評価値なので、
				// NegaMax()に渡すときに符号を反転する
				vmd_without_nega_max_child.value = static_cast<Value>(-book_move->value);
				vmd_without_nega_max_child.depth = static_cast<Depth>(book_move->depth);
				// 子局面の指し手なのでnextMoveを代入する
				vmd_without_nega_max_child.move = static_cast<Move>(book_move->nextMove);
			}

			StateInfo state_info = {};
			pos.do_move(move, state_info);
			auto value_and_depth_child = NegaMax(book, pos, vmd_without_nega_max_child, counter);
			pos.undo_move(move);

			if (vmd_without_nega_max_child.depth == DEPTH_NONE) {
				// 子局面が定跡データベースに登録されていなかった場合
				continue;
			}

			// 指し手情報に探索の結果を格納する
			// 返ってきた評価値は次の局面から見た評価値なので、符号を反転する
			BookPos new_book_move(To16Bit(move), To16Bit(value_and_depth_child.move), -value_and_depth_child.value,
				value_and_depth_child.depth, 1);
			book.insert(sfen, new_book_move);

			if (vmd.value < new_book_move.value) {
				vmd.value = static_cast<Value>(new_book_move.value);
				vmd.move = move;
			}
			// 最も深い探索深さ+1を設定する
			vmd.depth = std::max(vmd.depth, static_cast<Depth>(new_book_move.depth + 1));
		}

		return vmd;
	}
}

// 定跡データベースの末端局面の評価値をroot局面に向けて伝搬する
bool Tanuki::TeraShock() {
	std::string input_book_file = Options[kBookInputFile];
	std::string output_book_file = Options[kBookOutputFile];

	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	MemoryBook book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	book.read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book_file|=" << book.book_body.size() << sync_endl;

	// 平手の局面からたどれる局面について処理する
	auto& pos = Threads[0]->rootPos;
	StateInfo state_info = {};
	pos.set_hirate(&state_info, Threads[0]);
	ValueMoveDepth root_vmd = {};
	int counter = 0;
	NegaMax(book, pos, root_vmd, counter);

	// 平手の局面から辿れなかった局面を処理する
	for (const auto& book_entry : book.book_body) {
		auto& pos = Threads[0]->rootPos;
		StateInfo state_info = {};
		pos.set(book_entry.first, &state_info, Threads[0]);
		ValueMoveDepth root_vmd = {};
		NegaMax(book, pos, root_vmd, counter);
	}

	output_book_file = "book/" + output_book_file;
	book.write_book(output_book_file);
	sync_cout << "|output_book|=" << book.book_body.size() << sync_endl;

	return true;
}

// 定跡データベースの各局面の評価値を親局面に伝搬するのを繰り返す
bool Tanuki::TeraShock2() {
	std::string input_book_file = Options[kBookInputFile];
	std::string output_book_file = Options[kBookOutputFile];
	bool ignore_book_ply = Options["IgnoreBookPly"];

	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;
	sync_cout << "info string ignore_book_ply=" << ignore_book_ply << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	MemoryBook current_book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	current_book.read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book_file|=" << current_book.book_body.size() << sync_endl;

	// 1手ずつ伝搬させていく
	for (int depth = 1; depth <= 16; ++depth) {
		sync_cout << "depth:" << depth << sync_endl;

		// このMemoryBookに処理結果を追加していく
		MemoryBook next_book;

		int counter = 0;
		for (const auto& current_sfen_and_book_moves : current_book.book_body) {
			if (++counter % 100000 == 0) {
				sync_cout << counter << sync_endl;
			}

			const auto& current_sfen = current_sfen_and_book_moves.first;
			const auto& current_book_moves = *current_sfen_and_book_moves.second;

			Position& pos = Threads[0]->rootPos;
			StateInfo state_info0;
			pos.set(current_sfen, &state_info0, Threads[0]);

			for (auto& current_book_move : current_book_moves) {
				Move best_move = pos.move16_to_move(current_book_move.bestMove);
				if (!pos.pseudo_legal(best_move) ||
					!pos.legal(best_move)) {
					continue;
				}

				StateInfo state_info1;
				pos.do_move(best_move, state_info1);

				// MemoryBook::find()はIgnoreBookPlyに対応していないため、
				// MemoryBook::book_body::find()を直接呼び出す
				auto current_child_book_moves = current_book.book_body.find(
					RemovePlyIfIgnoreBookPly(pos.sfen()));
				if (current_child_book_moves == current_book.book_body.end()) {
					// 子局面が定跡データベースに含まれていなかった。
					// この指し手はそのまま追加する。
					pos.undo_move(best_move);

					// 現在の局面に対して指し手を登録するため、undo_move()してから登録する
					next_book.insert(RemovePlyIfIgnoreBookPly(pos.sfen()), current_book_move);
					continue;
				}

				// 子局面の指し手の中で最も評価値の高い指し手を選び
				// その評価値を現在の局面の指し手に伝搬する
				int best_value = -VALUE_MATE;
				Move next_move = MOVE_NONE;
				int depth = 0;
				for (const auto& current_child_book_move : *current_child_book_moves->second) {
					if (best_value < current_child_book_move.value) {
						best_value = current_child_book_move.value;
						next_move = current_child_book_move.bestMove;
					}
					depth = std::max(depth, current_child_book_move.depth);
				}

				pos.undo_move(best_move);

				// 現在の局面に対して指し手を登録するため、undo_move()してから登録する
				next_book.insert(RemovePlyIfIgnoreBookPly(pos.sfen()),
					BookPos(best_move, next_move, -best_value, depth, 1));
			}
		}

		std::swap(current_book, next_book);

		char output_book_file_for_this_depth[_MAX_PATH];
		sprintf(output_book_file_for_this_depth, "book/%s.%02d", output_book_file.c_str(), depth);
		current_book.write_book(output_book_file_for_this_depth);
		sync_cout << "|output_book|=" << current_book.book_body.size() << sync_endl;
	}

	return true;
}

// テラショック定跡を延長する
bool Tanuki::ExtendTeraShock() {
	int num_threads = (int)Options[kThreads];
	int multi_pv = (int)Options[kMultiPV];
	std::string input_book_file = Options[kBookInputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];
	std::string output_book_file = Options[kBookOutputFile];

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string multi_pv=" << multi_pv << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	BookMoveSelector book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	book.GetMemoryBook().read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book_file|=" << book.GetMemoryBook().book_body.size() << sync_endl;

	output_book_file = "book/" + output_book_file;

	time_t last_save_time_sec = std::time(nullptr);

	std::vector<std::thread> threads;
	std::set<std::string> searching_positions;
	std::mutex mutex;
	for (int thread_index = 0; thread_index < num_threads; ++thread_index) {
		auto procedure = [thread_index, multi_pv, search_depth, search_nodes, &book, &mutex,
			&searching_positions, &output_book_file, &last_save_time_sec]() {
			//sync_cout << "Thread " << thread_index << " started." << sync_endl;
			for (;;) {
				std::vector<StateInfo> state_info(512);
				std::vector<Move> moves;
				//int thread_index = 0;
				auto th = Threads[thread_index];
				auto& pos = th->rootPos;
				pos.set_hirate(&state_info[0], Threads[thread_index]);

				bool extend = false;
				{
					std::lock_guard<std::mutex> lock(mutex);

					for (;;) {
						auto book_moves = book.GetMemoryBook().find(pos);

						if (!book_moves) {
							// 定跡データベースの末端局面にたどり着いたので延長する
							extend = true;
							break;
						}

						if (book_moves->size() < multi_pv) {
							// 指し手の候補数が少ないので延長する
							extend = true;
							break;
						}

						auto move = book.probe(pos);
						if (move == MOVE_NONE) {
							// 手番側から見て不利な局面
							// この局面にたどり着いたらどうしようもないので延長しない
							break;
						}

						// 定跡の指し手に従って指し手を進める
						moves.push_back(move);
						pos.do_move(move, state_info[pos.game_ply()]);
					}
				}

				if (!extend) {
					continue;
				}

				//sync_cout << pos << sync_endl;

				{
					std::lock_guard<std::mutex> lock(mutex);
					if (searching_positions.count(pos.sfen())) {
						// 現在他のスレッドで探索中の場合は探索しない
						continue;
					}
					searching_positions.insert(pos.sfen());
				}

				// MultiPVで探索する
				sync_cout << "Searching sfen " << pos.sfen() << sync_endl;
				Learner::search(pos, search_depth, multi_pv, search_nodes);
				sync_cout << "Searched  sfen " << pos.sfen() << sync_endl;

				{
					std::lock_guard<std::mutex> lock(mutex);

					// 下のほうでposを変更するので、このタイミングで探索中フラグを下ろしておく。
					searching_positions.erase(pos.sfen());

					// 定跡データベースを更新する
					for (int pv_index = 0; pv_index < multi_pv && pv_index < th->rootMoves.size(); ++pv_index) {
						Move best_move = MOVE_NONE;
						if (th->rootMoves[pv_index].pv.size() > 0) {
							best_move = th->rootMoves[pv_index].pv[0];
						}

						Move next_move = MOVE_NONE;
						if (th->rootMoves[pv_index].pv.size() > 1) {
							next_move = th->rootMoves[pv_index].pv[1];
						}

						int value = th->rootMoves[pv_index].score;
						int depth = th->completedDepth;
						// 既存の指し手はには、定跡ツリー探索後の評価値が入っている可能性があるので、
						// 上書きしないようにする
						book.GetMemoryBook().insert(pos.sfen(), BookPos(best_move, next_move, value, depth, 1), false);
					}

					// 平手の局面に向かって評価値を伝搬する
					while (!moves.empty())
					{
						// この局面の評価値を求める
						auto pos_move_list = book.GetMemoryBook().find(pos);

						// この局面は必ず見つかるはず
						ASSERT_LV3(pos_move_list);

						Value best_value = -VALUE_MATE;
						Depth best_depth = DEPTH_ZERO;
						// 一手前の局面の指し手に対する応手となる
						Move next_move = MOVE_NONE;
						for (const auto& book_move : *pos_move_list) {
							if (best_value >= book_move.value) {
								continue;
							}
							best_value = static_cast<Value>(book_move.value);
							best_depth = static_cast<Depth>(book_move.depth);
							next_move = static_cast<Move>(book_move.bestMove);
						}

						// 指し手は必ず登録されているはず
						ASSERT_LV3(best_value != -VALUE_MATE);

						// 一手戻す
						Move best_move = moves.back();
						pos.undo_move(best_move);
						moves.pop_back();

						pos_move_list = book.GetMemoryBook().find(pos);

						// この局面は必ず見つかるはず
						ASSERT_LV3(pos_move_list);

						auto book_move = std::find_if(pos_move_list->begin(), pos_move_list->end(),
							[best_move, &pos](const auto& x) {
								return pos.move16_to_move(best_move) ==
									pos.move16_to_move(x.bestMove);
							});

						// 指し手は必ず見つかるはず
						ASSERT_LV3(book_move != pos_move_list->end());

						book_move->nextMove = next_move;
						book_move->depth = best_depth + 1;
						book_move->value = -best_value;
					}

					// 定跡をストレージに書き出す。
					if (last_save_time_sec + kSavePerAtMostSec < std::time(nullptr)) {
						book.GetMemoryBook().write_book(output_book_file);
						last_save_time_sec = std::time(nullptr);
						sync_cout << "|output_book_file|=" << book.GetMemoryBook().book_body.size() << sync_endl;
					}
				}
			}
		};
		threads.push_back(std::thread(procedure));
	}

	for (auto& thread : threads) {
		thread.join();
	}

	return true;
}

// テラショック定跡を延長する
bool Tanuki::ExtendTeraShockBfs() {
	int num_threads = (int)Options[kThreads];
	int multi_pv = (int)Options[kMultiPV];
	std::string input_book_file = Options[kBookInputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];
	std::string output_book_file = Options[kBookOutputFile];
	int book_depth_limit = (int)Options["BookDepthLimit"];
	int book_eval_diff = (int)Options["BookEvalDiff"];
	int book_eval_black_limit = (int)Options["BookEvalBlackLimit"];
	int book_eval_white_limit = (int)Options["BookEvalWhiteLimit"];

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string multi_pv=" << multi_pv << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;
	sync_cout << "info string book_depth_limit=" << book_depth_limit << sync_endl;
	sync_cout << "info string book_eval_diff=" << book_eval_diff << sync_endl;
	sync_cout << "info string book_eval_black_limit=" << book_eval_black_limit << sync_endl;
	sync_cout << "info string book_eval_white_limit=" << book_eval_white_limit << sync_endl;

	Search::LimitsType limits;
	// 引き分けの手数付近で引き分けの値が返るのを防ぐため1 << 16にする
	limits.max_game_ply = 1 << 16;
	limits.depth = MAX_PLY;
	limits.silent = true;
	limits.enteringKingRule = EKR_27_POINT;
	Search::Limits = limits;

	BookMoveSelector book;
	input_book_file = "book/" + input_book_file;
	sync_cout << "Reading input book file: " << input_book_file << sync_endl;
	book.GetMemoryBook().read_book(input_book_file);
	sync_cout << "done..." << sync_endl;
	sync_cout << "|input_book_file|=" << book.GetMemoryBook().book_body.size() << sync_endl;

	output_book_file = "book/" + output_book_file;

	std::random_device rnd;
	std::mt19937_64 mt(rnd());

	for (int iteration = 1;; ++iteration) {
		sync_cout << "Iteration " << iteration << " started." << sync_endl;
		sync_cout << "Enumerating target positions." << sync_endl;

		// 延長対象の局面。
		// 経路を保持するため、平手局面からの指し手の配列として持たせる。
		// Moveは32ビット版を格納するようにする。
		std::vector<std::vector<Move>> target_positions;

		// 炎症対象の局面を探索する際、すでに探索した局面を表す集合。
		// 途中の経路に依存しないようにするためsfen形式で持たせる。
		std::set<std::string> visited;

		auto& position = Threads.main()->rootPos;
		std::vector<StateInfo> state_info(1024);
		position.set_hirate(&state_info[0], Threads.main());

		// 幅優先探索により延長対象の局面を探索する。
		// 平手局面から開始する。
		std::queue<std::vector<Move>> q;
		q.push(std::vector<Move>());
		while (!q.empty()) {
			std::vector<Move> moves = q.front();
			q.pop();

			// 現在の局面まで進める
			for (auto move : moves) {
				position.do_move(move, state_info[position.game_ply()]);
			}

			std::string sfen = position.sfen();
			//sync_cout << position << sync_endl;

			auto book_moves = book.GetMemoryBook().find(position);

			if (book_moves == nullptr || book_moves->size() < multi_pv) {
				// 定跡データベースに登録されていない場合、
				// または登録されている指し手の数が少ない場合、
				// 探索対象に加える。
				target_positions.push_back(moves);
			}
			
			if (book_moves && position.game_ply() <= iteration) {
				// 探索する手数に差がありすぎる場合、評価値の絶対値の平均が大きく離れてしまう可能性がある。
				// この場合、手数の小さい局面より大きい局面の評価値が優先されてしまう可能性がある。
				// これを防ぐため、定跡データベースに指し手が含まれており、
				// かつ手数がiterationより小さい場合のみ展開するようにする。
				std::vector<BookPos> valid_book_moves(book_moves->begin(), book_moves->end());

				// Moveを16ビットから32ビットに変換する。
				for (auto& move : valid_book_moves) {
					move.bestMove = position.move16_to_move(move.bestMove);
				}

				// 非合法手・評価値が低すぎる指し手・探索深さが低すぎる指し手を取り除く。
				int book_eval_limit = (position.side_to_move() == BLACK) ?
					book_eval_black_limit : book_eval_white_limit;
				valid_book_moves.erase(std::remove_if(
					valid_book_moves.begin(), valid_book_moves.end(),
					[book_eval_limit, book_depth_limit, &position](const BookPos& book_move) {
						return
							!position.pseudo_legal(book_move.bestMove) ||
							!position.legal(book_move.bestMove) ||
							book_move.value < book_eval_limit ||
							book_move.depth < book_depth_limit;
					}), valid_book_moves.end());

				// 指し手の評価値の最大値を求める。
				int best_value = -VALUE_MATE;
				for (auto book_move : valid_book_moves) {
					best_value = std::max(best_value, book_move.value);
				}

				// 指し手の評価値の最大値から評価値が離れすぎている指し手を取り除く。
				valid_book_moves.erase(std::remove_if(
					valid_book_moves.begin(), valid_book_moves.end(),
					[book_eval_diff, best_value](const BookPos& book_move) {
						return book_move.value < best_value - book_eval_diff;
					}), valid_book_moves.end());

				// 指し手をシャッフルし、探索のルートをランダムにする。
				std::shuffle(valid_book_moves.begin(), valid_book_moves.end(), mt);

				// キューに局面を加える。
				for (const auto& move : valid_book_moves) {
					position.do_move(move.bestMove, state_info[position.game_ply()]);
					//sync_cout << position << sync_endl;
					std::string next_sfen = position.sfen();
					position.undo_move(move.bestMove);

					// 一度キューに加えた局面は加えない。
					if (visited.find(next_sfen) != visited.end()) {
						continue;
					}
					visited.insert(next_sfen);

					auto next_moves = moves;
					next_moves.push_back(move.bestMove);
					q.push(next_moves);
				}
			}

			// 平手の局面まで戻す
			for (auto rit = moves.rbegin(); rit != moves.rend(); ++rit) {
				position.undo_move(*rit);
			}
		}

		sync_cout << "Expanding " << target_positions.size() << " positions." << sync_endl;

		// マルチスレッドで、探索対象局面を探索する。
		std::vector<std::thread> threads;
		std::atomic_int global_position_index;
		global_position_index = 0;
		std::mutex mutex;
		time_t last_save_time_sec = std::time(nullptr);
		for (int thread_index = 0; thread_index < num_threads; ++thread_index) {
			auto procedure = [thread_index, multi_pv, search_depth, search_nodes, &book, &mutex,
				&target_positions, &output_book_file, &last_save_time_sec,
				&global_position_index]() {
				//sync_cout << "Thread " << thread_index << " started." << sync_endl;
				for (int position_index = global_position_index++;
					position_index < target_positions.size();
					position_index = global_position_index++) {
					std::vector<StateInfo> state_info(512);
					// 平手の局面から対象の局面までの指し手。
					// あとで後ろのほうから要素を削除していくので、コピーを作成しておく。
					std::vector<Move> moves = target_positions[position_index];
					auto thread = Threads[thread_index];
					auto& position = thread->rootPos;
					position.set_hirate(&state_info[0], thread);

					for (auto move : moves) {
						position.do_move(move, state_info[position.game_ply()]);
					}

					//sync_cout << pos << sync_endl;

					// MultiPVで探索する
					sync_cout << "sfen " << position.sfen() << sync_endl;
					Learner::search(position, search_depth, multi_pv, search_nodes);

					{
						std::lock_guard<std::mutex> lock(mutex);

						// 定跡データベースを更新する
						for (int pv_index = 0; pv_index < multi_pv && pv_index < thread->rootMoves.size(); ++pv_index) {
							Move best_move = MOVE_NONE;
							if (thread->rootMoves[pv_index].pv.size() > 0) {
								best_move = thread->rootMoves[pv_index].pv[0];
							}

							Move next_move = MOVE_NONE;
							if (thread->rootMoves[pv_index].pv.size() > 1) {
								next_move = thread->rootMoves[pv_index].pv[1];
							}

							int value = thread->rootMoves[pv_index].score;
							int depth = thread->completedDepth;
							// 既存の指し手はには、評価値伝搬後の評価値が入っている場合があるので、
							// 上書きしないようにする。
							book.GetMemoryBook().insert(position.sfen(), BookPos(best_move, next_move, value, depth, 1), false);
						}

						// 平手の局面に向かって評価値を伝搬する
						while (!moves.empty())
						{
							// この局面の評価値を求める
							auto pos_move_list = book.GetMemoryBook().find(position);

							// この局面は必ず見つかるはず
							ASSERT_LV3(pos_move_list);

							Value best_value = -VALUE_MATE;
							Depth best_depth = DEPTH_ZERO;
							// 一手前の局面の指し手に対する応手となる
							Move next_move = MOVE_NONE;
							for (const auto& book_move : *pos_move_list) {
								if (best_value >= book_move.value) {
									continue;
								}
								best_value = static_cast<Value>(book_move.value);
								best_depth = static_cast<Depth>(book_move.depth);
								next_move = static_cast<Move>(book_move.bestMove);
							}

							// 指し手は必ず登録されているはず
							ASSERT_LV3(best_value != -VALUE_MATE);

							// 一手戻す
							Move best_move = moves.back();
							moves.pop_back();
							position.undo_move(best_move);

							pos_move_list = book.GetMemoryBook().find(position);

							// この局面は必ず見つかるはず
							ASSERT_LV3(pos_move_list);

							auto book_move = std::find_if(pos_move_list->begin(), pos_move_list->end(),
								[best_move, &position](const auto& x) {
									return position.move16_to_move(best_move) ==
										position.move16_to_move(x.bestMove);
								});

							// 指し手は必ず見つかるはず
							ASSERT_LV3(book_move != pos_move_list->end());

							book_move->nextMove = next_move;
							book_move->depth = best_depth + 1;
							book_move->value = -best_value;
						}

						// 定跡をストレージに書き出す。
						if (last_save_time_sec + kSavePerAtMostSec < std::time(nullptr)) {
							std::string backup_file_path = output_book_file + ".bak";

							sync_cout << "Removing the backup file. backup_file_path=" << backup_file_path << sync_endl;
							std::filesystem::remove(backup_file_path);

							sync_cout << "Renaming the output file. output_book_file=" << output_book_file << " backup_file_path=" << backup_file_path << sync_endl;
							std::filesystem::rename(output_book_file, backup_file_path);

							book.GetMemoryBook().write_book(output_book_file);
							last_save_time_sec = std::time(nullptr);
							sync_cout << "|output_book_file|=" << book.GetMemoryBook().book_body.size() << sync_endl;
						}
					}
				}
			};
			threads.push_back(std::thread(procedure));
		}

		for (auto& thread : threads) {
			thread.join();
		}

		// 定跡をストレージに書き出す。
		std::string backup_file_path = output_book_file + ".bak";

		sync_cout << "Removing the backup file. backup_file_path=" << backup_file_path << sync_endl;
		std::filesystem::remove(backup_file_path);

		sync_cout << "Renaming the output file. output_book_file=" << output_book_file << " backup_file_path=" << backup_file_path << sync_endl;
		std::filesystem::rename(output_book_file, backup_file_path);

		book.GetMemoryBook().write_book(output_book_file);
		last_save_time_sec = std::time(nullptr);
		sync_cout << "|output_book_file|=" << book.GetMemoryBook().book_body.size() << sync_endl;
	}

	return true;
}
#endif
