#include "util.h"
#include "log.h"

#include <string>
#include <vector>
#include <fstream>
#include <streambuf>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <climits>
#include <cstdlib>

#include <glob.h>
#include <sys/stat.h>

std::string file_read_str(const std::string& filename)
{
	std::string str;
	std::ifstream ifs;
	ifs.open(filename, std::ios::binary);
	if (!ifs.is_open())
	{
		LOG("ifs.open() failed:%s", std::strerror(errno));
		return str;
	}
	ifs.seekg(0, std::ios::end);
	str.reserve(ifs.tellg());
	ifs.seekg(0, std::ios::beg);
	str.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	return str;
}

bool file_write_str(const std::string& filename, std::string str)
{
	std::ofstream ofs;
	ofs.open(filename, std::ios::binary);
	if (!ofs.is_open())
	{
		LOG("ofs.open() failed:%s", std::strerror(errno));
		return false;
	}
	ofs.write(str.c_str(), str.length());
	ofs.close();
	return true;
}

str_list_t file_read_str_list(const std::string& filename)
{
	str_list_t str_list;

	std::ifstream ifs;
	ifs.open(filename, std::ios::binary);
	if (!ifs.is_open())
		return str_list;

	std::string line;
	while (std::getline(ifs, line, '\n'))
		str_list.push_back(line);

	ifs.close();

	return str_list;
}

std::string file_line_match(std::ifstream& ifs, const std::string& pattern)
{
	ifs.seekg(0);
	std::string line;
	while (std::getline(ifs, line, '\n'))
	{
		if (line.find(pattern) != std::string::npos)
			break;
	}
	return line;
}

std::string file_line_match(const std::string& file_path, const std::string& pattern)
{
	std::ifstream ifs;
	ifs.open(file_path, std::ios::binary);
	if (!ifs.is_open())
		return std::string();
	std::string str = file_line_match(ifs, pattern);
	ifs.close();
	return str;
}

key_val_t key_val_split(const std::string& str, const std::string& delimiter)
{
	key_val_t key_val = {};
	key_val.key = str;
	size_t pos = str.find(delimiter);
	if (pos == std::string::npos)
		return key_val;
	key_val.key = str.substr(0, pos);
	pos = str.find_first_not_of(" \t", pos + delimiter.length());
	if (pos == std::string::npos)
		return key_val;
	key_val.val = str.substr(pos, str.length());
	return key_val;
}

fs_path_list_t glob_path_list(const char* glob_pattern)
{
	glob_t glob_result = {};
	int ret;
	(void)ret;
	ret = glob(glob_pattern, 0, nullptr, &glob_result);
	fs_path_list_t path_list;
	for (size_t idx = 0; idx < glob_result.gl_pathc; ++idx)
		path_list.push_back(std::string(glob_result.gl_pathv[idx]));

	globfree(&glob_result);
	return path_list;
}

std::string basename(const std::string& path)
{
	if (path.empty())
		return std::string();
	size_t last_slash_pos = path.rfind('/');
	if (last_slash_pos == std::string::npos)
		return path.c_str();

	if (last_slash_pos == path.length() - 1)
		last_slash_pos = path.rfind('/');
	if (last_slash_pos == std::string::npos)
		return path.c_str();

	return path.substr(last_slash_pos + 1, path.length() - last_slash_pos);
}

std::string dirname(const std::string& path)
{
	if (path.empty())
		return std::string();
	size_t last_slash_pos = path.rfind('/');
	if (last_slash_pos == std::string::npos)
		return std::string(".");

	if (last_slash_pos == path.length() - 1)
		last_slash_pos = path.rfind('/');
	if (last_slash_pos == std::string::npos)
		return std::string(".");

	return path.substr(0, last_slash_pos);
}

std::string realpath_str(const std::string& path)
{
	std::string rpath;
	char buf[1024];
	if (::realpath(path.c_str(), buf) == nullptr)
		return rpath;
	rpath = buf;
	return rpath;
}

str_list_t split_str(const std::string &str, const std::string& delim)
{
	str_list_t list;
	key_val_t key_val = {};

	key_val = key_val_split(str, delim);
	list.push_back(key_val.key);
	while(!key_val.val.empty())
	{
		key_val = key_val_split(key_val.val, delim);
		list.push_back(key_val.key);
	}

	return list;
}

bool mkdir_p(std::string path)
{
	str_list_t dir_list = split_str(path, "/");

	std::string cur_path = {};
	for (auto dir_part : dir_list)
	{
		if (!cur_path.empty())
			cur_path += "/";
		cur_path += dir_part;
		struct stat info = {};
		stat(cur_path.c_str(), &info);
		if (!(info.st_mode & S_IFDIR))
		{
			if (0 != mkdir(cur_path.c_str(), 0777))  // S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH
			{
				LOG("mkdir(%s) failed:%s", cur_path.c_str(), std::strerror(errno));
				return false;
			}
		}
	}
	return true;
}

bool str_starts_with(const std::string& str, const std::string& start)
{
	return str.compare(0, start.length(), start) == 0;
}

bool str_ends_with(const std::string& str, const std::string& end)
{
	if (str.length() < end.length())
		return false;
	return str.compare(str.length() - end.length(), end.length(), end) == 0;
}

std::string str_trim(std::string str)
{
	if (str.back() == '\n')  //TODO: use std::isspace()
		str.erase(str.length() - 1);
	return str;
}
