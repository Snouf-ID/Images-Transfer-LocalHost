#pragma once
#include <string>

class WindowsFileDiag
{
public:
	static std::string select_folder();
	static void apply_last_modified(const std::string& file_path, double last_modified_time);
	static void setFileCreationTime(const std::string& filePath, uint64_t creationTimeMs);
};

