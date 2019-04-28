#ifndef _TANUKI_BOOK_H_
#define _TANUKI_BOOK_H_

#include "extra/config.h"

#ifdef EVAL_LEARN

#include "shogi.h"

namespace Tanuki {
	bool InitializeBook(USI::OptionsMap& o);
	bool CreateRawBook();
	bool CreateScoredBook();
	bool ExtendBook();
}

#endif

#endif
