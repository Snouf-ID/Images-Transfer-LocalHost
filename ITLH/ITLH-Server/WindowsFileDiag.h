#pragma once
#include <string>

class WindowsFileDiag
{
public:
	static std::string open_select_folder_diag_window();
	static void apply_last_modified_date_on_file(const std::string& file_path, double last_modified_time);
	static void apply_metadata_date_on_file(const std::string& filePath);
};

