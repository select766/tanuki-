#ifndef _TANUKI_BOOK_H_
#define _TANUKI_BOOK_H_

#include "config.h"

#ifdef EVAL_LEARN

#include "usi.h"

namespace Tanuki {
	bool InitializeBook(USI::OptionsMap& o);
	bool CreateRawBook();
	bool CreateScoredBook();
	bool ExtendBook();
	bool MergeBook();
	bool SetScoreToMove();
	bool TeraShock();
}

#endif

#endif
