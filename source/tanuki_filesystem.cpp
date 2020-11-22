#include "tanuki_filesystem.h"

#include <fstream>

bool Tanuki::IsRegularFile(const std::string& file_path)
{
	std::ifstream ifs(file_path);
	return ifs.is_open();
}

void Tanuki::CopyFile(const std::string& from, const std::string& to)
{
	std::ifstream ifs(from);
	std::ofstream ofs(to);
	std::string line;
	while (std::getline(ifs, line)) {
		ofs << line << std::endl;
	}
}
