#include "tanuki_s_book_black_start_position_picker.h"

#include "thread.h"
#include "usi.h"

using Tanuki::SBookBlackStartPositionPicker;

namespace {
	constexpr const char* kOptionGeneratorStartposFileName = "GeneratorStartposFileName";
	constexpr const char* kOptionGeneratorStartPositionMaxPlay = "GeneratorStartPositionMaxPlay";
}

bool SBookBlackStartPositionPicker::Open()
{
	// s-book_black を読み込む。
	std::string file_path = Options[kOptionGeneratorStartposFileName];
	sync_cout << "info string Reading s-book_back. file_path=" << file_path << sync_endl;
	if (book_.read_book(file_path).is_not_ok()) {
		sync_cout << "info string Failed to read s-book_black. file_path=" << file_path << sync_endl;
		return false;
	}

	// 開始位置を初期化する。
	book_iterator_ = book_.get_body().begin();

	// 先手番の局面を探す
	while (book_iterator_ != book_.get_body().end() &&
		book_iterator_->first.find(" b ") == std::string::npos) {
		++book_iterator_;
	}
	ASSERT_LV3(book_iterator_ != book_.get_body().end());

	book_move_iterator_ = book_iterator_->second->begin();
	// 各局面に 1 手以上指し手が登録されていると仮定する
	ASSERT_LV3(book_move_iterator_ != book_iterator_->second->end());
	return true;
}

void SBookBlackStartPositionPicker::Pick(Position& position, StateInfo*& state_info, Thread& thread)
{
	std::lock_guard<std::mutex> lock(mutex_);
	position.set(book_iterator_->first, state_info++, &thread);

	auto move = position.to_move(book_move_iterator_->move);
	ASSERT_LV3(position.pseudo_legal(move));
	ASSERT_LV3(position.legal(move));

	position.do_move(move, *state_info++);

	// 次の指し手候補に進める。
	++book_move_iterator_;

	if (book_move_iterator_ != book_iterator_->second->end()) {
		return;
	}

	// 指し手候補が無かったら、次の局面に進める。
	++book_iterator_;

	// 先手番の局面を探す
	while (book_iterator_ != book_.get_body().end() &&
		book_iterator_->first.find(" b ") == std::string::npos) {
		++book_iterator_;
	}

	if (book_iterator_ == book_.get_body().end()) {
		// すべての局面から選び終えたら、最初の局面に戻る。
		book_iterator_ = book_.get_body().begin();

		// 先手番の局面を探す
		while (book_iterator_ != book_.get_body().end() &&
			book_iterator_->first.find(" b ") == std::string::npos) {
			++book_iterator_;
		}

		ASSERT_LV3(book_iterator_ != book_.get_body().end());
	}

	book_move_iterator_ = book_iterator_->second->begin();
	// 各局面に 1 手以上指し手が登録されていると仮定する
	ASSERT_LV3(book_move_iterator_ != book_iterator_->second->end());
}
