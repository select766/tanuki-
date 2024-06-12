#ifndef _TANUKI_S_BOOK_BLACK_START_POSITION_PICKER_H_
#define _TANUKI_S_BOOK_BLACK_START_POSITION_PICKER_H_

#ifdef EVAL_LEARN

#include <random>

#include "book/book.h"
#include "config.h"
#include "position.h"
#include "tanuki_start_position_picker.h"

namespace Tanuki {
	class SBookBlackStartPositionPicker : public StartPositionPicker
	{
	public:
		virtual bool Open() override;
		virtual void Pick(Position& position, StateInfo*& state_info, Thread& thread) override;

	private:
		Book::MemoryBook book_;
		Book::BookType::iterator book_iterator_;
		Book::BookMoveIterator book_move_iterator_;
		std::mutex mutex_;
	};
}

#endif

#endif
