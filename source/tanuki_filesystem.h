#ifndef _TANUKI_FILESYSTEM_H_
#define _TANUKI_FILESYSTEM_H_

#include <string>

namespace Tanuki {
	bool IsRegularFile(const std::string& file_path);
	void CopyFile(const std::string& from, const std::string& to);
}

#endif
