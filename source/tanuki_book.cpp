#include "tanuki_book.h"
#include "config.h"

#ifdef EVAL_LEARN

#include <atomic>
#include <fstream>
#include <sstream>
#include <set>
#include <ctime>

#include "extra/book/book.h"
#include "misc.h"
#include "position.h"
#include "tanuki_progress_report.h"
#include "thread.h"
#include "learn/learn.h"

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
	constexpr const char* kBookSavePerPositions = "BookSavePerPositions";
	constexpr const char* kThreads = "Threads";
	constexpr const char* kMultiPV = "MultiPV";
	constexpr const char* kBookOverwriteExistingPositions = "OverwriteExistingPositions";
	constexpr const char* kBookNarrowBook = "NarrowBook";
	//constexpr int kShowProgressAtMostSec = 1 * 60;	// 1分
	constexpr int kShowProgressAtMostSec = 60 * 60;	// 1時間
	constexpr int kSaveEvalAtMostSec = 60 * 60;		// 1時間
}

bool Tanuki::InitializeBook(USI::OptionsMap& o) {
	o[kBookSfenFile] << Option("merged.sfen");
	o[kBookMaxMoves] << Option(32, 0, 256);
	o[kBookSearchDepth] << Option(64, 0, 256);
	o[kBookSearchNodes] << Option(500000 * 60, 0, INT_MAX);
	o[kBookInputFile] << Option("user_book1.db");
	o[kBookOutputFile] << Option("user_book2.db");
	o[kBookSavePerPositions] << Option(1000, 1, std::numeric_limits<int>::max());
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

	sync_cout << "info string Writing book file..." << sync_endl;

	std::string book_file = Options[kBookFile];
	book_file = "book/" + book_file;
	memory_book.write_book(book_file);

	sync_cout << "info string Done..." << sync_endl;

	return true;
}

bool Tanuki::CreateScoredBook() {
	int num_threads = (int)Options[kThreads];
	std::string input_book_file = Options[kBookInputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];
	int multi_pv = (int)Options[kMultiPV];
	std::string output_book_file = Options[kBookOutputFile];
	int save_per_positions = (int)Options[kBookSavePerPositions];
	bool overwrite_existing_positions = static_cast<bool>(Options[kBookOverwriteExistingPositions]);

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string multi_pv=" << multi_pv << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;
	sync_cout << "info string save_per_positions=" << save_per_positions << sync_endl;
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
	ProgressReport progress_report(num_sfens, kShowProgressAtMostSec);

	std::vector<std::thread> threads;
	std::atomic_int global_pos_index;
	global_pos_index = 0;
	std::atomic_int global_num_processed_positions;
	global_num_processed_positions = 0;
	for (int thread_index = 0; thread_index < num_threads; ++thread_index) {
		threads.push_back(std::thread([thread_index, num_sfens, search_depth, multi_pv, search_nodes,
			save_per_positions, &global_pos_index, &sfens, &output_book,
			&output_book_mutex, &progress_report, &output_book_file,
			&global_num_processed_positions]() {
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

					int num_processed_positions = ++global_num_processed_positions;
					progress_report.Show(num_processed_positions);

					if (num_processed_positions && num_processed_positions % save_per_positions == 0) {
						std::lock_guard<std::mutex> lock(output_book_mutex);
						sync_cout << "Writing the book file..." << sync_endl;
						output_book.write_book(output_book_file);
						sync_cout << "done..." << sync_endl;
					}
				}
			}));
	}

	for (auto& thread : threads) {
		thread.join();
	}

	sync_cout << "Writing the book file..." << sync_endl;
	output_book.write_book(output_book_file);
	sync_cout << "done..." << sync_endl;

	return true;
}

bool Tanuki::ExtendBook() {
	int num_threads = (int)Options[kThreads];
	std::string input_file = Options[kBookInputFile];
	std::string output_file = Options[kBookOutputFile];
	int search_depth = (int)Options[kBookSearchDepth];
	int search_nodes = (int)Options[kBookSearchNodes];
	int save_per_positions = (int)Options[kBookSavePerPositions];

	// スレッド0版を未展開のノードを探すために、
	// それ以外のスレッドを未展開のノードの指し手を決定するために使うため
	// 2スレッド以上必要となる。
	ASSERT_LV3(num_threads >= 2);

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string input_file=" << input_file << sync_endl;
	sync_cout << "info string output_file=" << output_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string save_per_positions=" << save_per_positions << sync_endl;

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

	time_t last_save_time = 0;
	std::time(&last_save_time);

	std::mutex sfens_mutex;
	std::mutex output_mutex;
	std::condition_variable cv;

	std::vector<std::thread> threads;
	std::set<std::string> sfens_to_be_processed;
	std::set<std::string> sfens_processing;
	// スレッド0番は未展開のノードを探すために使う
	// スレッド1番以降を未展開のノードの指し手を決定するために使う
	for (int thread_index = 1; thread_index < num_threads; ++thread_index) {
		threads.push_back(std::thread([thread_index, search_depth, output_file, save_per_positions,
			&last_save_time, &sfens_to_be_processed, &sfens_processing,
			&input_book, &output_book, &sfens_mutex, &output_mutex,
			&cv]() {
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

					time_t now = std::time(nullptr);
					if (now - last_save_time < kSaveEvalAtMostSec) {
						continue;
					}

					sync_cout << "Writing the book file..." << sync_endl;
					// 上書き保存する
					output_book.GetMemoryBook().write_book("book/" + output_file);
					last_save_time = now;
					sync_cout << "done..." << sync_endl;
				}
			}));
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

	std::istringstream iss(input_file_list);
	std::string input_file;
	while (std::getline(iss, input_file, ';')) {
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
				output_book.GetMemoryBook().insert(sfen, pos_move);
			}
		}
	}

	sync_cout << "Writing the book file..." << sync_endl;
	output_book.GetMemoryBook().write_book("book/" + output_file);
	sync_cout << "done..." << sync_endl;

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
	int save_per_positions = (int)Options[kBookSavePerPositions];
	bool overwrite_existing_positions = static_cast<bool>(Options[kBookOverwriteExistingPositions]);

	sync_cout << "info string num_threads=" << num_threads << sync_endl;
	sync_cout << "info string input_book_file=" << input_book_file << sync_endl;
	sync_cout << "info string search_depth=" << search_depth << sync_endl;
	sync_cout << "info string search_nodes=" << search_nodes << sync_endl;
	sync_cout << "info string output_book_file=" << output_book_file << sync_endl;
	sync_cout << "info string save_per_positions=" << save_per_positions << sync_endl;
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

	// 既に探索した局面については探索しない
	for (const auto& sfen_and_pos_move_list : input_book.book_body) {
		if (!overwrite_existing_positions &&
			output_book.book_body.find(sfen_and_pos_move_list.first) != output_book.book_body.end()) {
			continue;
		}
		sfens.push_back(sfen_and_pos_move_list.first);
	}
	int num_sfens = sfens.size();
	sync_cout << "Number of the positions to be processed: " << num_sfens << sync_endl;

	// 定跡保存周期調整のため
	time_t start_time = 0;
	std::time(&start_time);

	// マルチスレッド処理準備のため、インデックスを初期化する
	std::atomic_int global_position_index;
	global_position_index = 0;
	std::mutex output_book_mutex;

	// 進捗状況表示の準備
	ProgressReport progress_report(num_sfens, kShowProgressAtMostSec);

	// Apery式マルチスレッド処理
	std::vector<std::thread> threads;
	std::atomic_int global_pos_index;
	global_pos_index = 0;
	std::atomic_int global_num_processed_positions;
	global_num_processed_positions = 0;
	for (int thread_index = 0; thread_index < num_threads; ++thread_index) {
		auto procedure = [thread_index, num_sfens, search_depth, search_nodes,
			save_per_positions, &global_pos_index, &sfens, &output_book, &output_book_mutex,
			&progress_report, &output_book_file,
			&global_num_processed_positions, &input_book]() {
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

				auto sfen_and_pos_move_list_ptr = input_book.book_body.find(sfen);
				if (sfen_and_pos_move_list_ptr == input_book.book_body.end()) {
					sync_cout << "sfen not found. sfen=" << sfen << sync_endl;
					continue;
				}

				for (auto& pos_move : *sfen_and_pos_move_list_ptr->second) {
					// 定跡データベースに含まれているmoveは16ビットのため、
					// このタイミングで32ビットに変換する。
					pos_move.bestMove = pos.move16_to_move(pos_move.bestMove);

					if (!pos.pseudo_legal(pos_move.bestMove) || !pos.legal(pos_move.bestMove)) {
						sync_cout << "Illegal move. sfen=" << sfen << " move=" << pos_move.bestMove << sync_endl;
						continue;
					}

					StateInfo state_info0;
					pos.do_move(pos_move.bestMove, state_info0);
					Eval::evaluate_with_no_return(pos);

					// この局面について探索する
					auto value_and_pv = Learner::search(pos, search_depth, 1, search_nodes);

					auto value = value_and_pv.first;
					auto pv = value_and_pv.second;

					if (pos_move.nextMove == Move::MOVE_NONE && pv.size() >= 1) {
						// 定跡の手を指した次の局面なので、nextMoveにはpv[0]を代入する。
						// ただし、もともとnextMoveが設定されている場合、それを優先する。
						pos_move.nextMove = pv[0];
					}

					// ひとつ前の局面から見た評価値を代入する必要があるので、符号を反転する。
					pos_move.value = -value;

					pos_move.depth = pos.this_thread()->completedDepth;

					// 定跡の局面に戻す
					pos.undo_move(pos_move.bestMove);
				}

				// 出力先の定跡に登録する
				// 1手ずつ登録すると中途半端なタイミングでストレージに書かれる場合があり、
				// 中断した際に一部の手が書きだされない場合がある。
				// そのため、まとめて登録する
				{
					std::lock_guard<std::mutex> lock(output_book_mutex);
					for (auto& pos_move : *sfen_and_pos_move_list_ptr->second) {
						output_book.insert(sfen, pos_move);
					}
				}

				// 進捗状況を表示する
				int num_processed_positions = ++global_num_processed_positions;
				progress_report.Show(num_processed_positions);

				// 定跡をストレージに書き出す。
				if (num_processed_positions && num_processed_positions % save_per_positions == 0) {
					std::lock_guard<std::mutex> lock(output_book_mutex);
					sync_cout << "Writing the book file..." << sync_endl;
					output_book.write_book(output_book_file);
					sync_cout << "done..." << sync_endl;
				}
			}
		};
		threads.push_back(std::thread(procedure));
	}

	for (auto& thread : threads) {
		thread.join();
	}

	sync_cout << "Writing the book file..." << sync_endl;
	output_book.write_book(output_book_file);
	sync_cout << "done..." << sync_endl;

	return true;
}

#endif
