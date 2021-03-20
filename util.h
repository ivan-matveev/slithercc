#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>

typedef std::vector<std::string> str_list_t;
typedef std::vector<std::string> fs_path_list_t;

typedef struct
{
	std::string key;
	std::string val;
} key_val_t;

std::string file_read_str(const std::string& filename);
bool file_write_str(const std::string& filename, std::string str);
str_list_t file_read_str_list(const std::string& filename);
std::string file_line_match(std::ifstream& ifs, const std::string& pattern);
std::string file_line_match(const std::string& file_path, const std::string& pattern);
key_val_t key_val_split(const std::string& str, const std::string& delimiter);
fs_path_list_t glob_path_list(const char* glob_pattern);
std::string basename(const std::string& path);
std::string dirname(const std::string& path);
std::string realpath_str(const std::string& path);
bool mkdir_p(std::string path);
bool str_starts_with(const std::string& str, const std::string& start);
bool str_ends_with(const std::string& str, const std::string& end);
std::string str_trim(std::string str);
str_list_t split_str(const std::string &str, const std::string& delim);

#endif  // #ifndef UTIL_H
