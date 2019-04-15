#ifndef _TANUKI_BOOK_H_
#define _TANUKI_BOOK_H_

#include "shogi.h"

namespace Tanuki {
	bool InitializeBook(USI::OptionsMap& o);
	bool CreateRawBook();
	bool CreateScoredBook();
	bool ExtendBook();
}

#endif
