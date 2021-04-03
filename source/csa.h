#ifndef CSA_H_INCLUDED
#define CSA_H_INCLUDED

#include <string>

#include "types.h"

namespace CSA {
	Move to_move(Position& pos, const std::string& str);
}

#endif // #ifndef CSA_H_INCLUDED
