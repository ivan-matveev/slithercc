#ifndef DECODE_SECRET_H
#define DECODE_SECRET_H

#include <array>
#include <string>
#include <vector>

typedef std::array<uint8_t, 24> secret_result_t;
secret_result_t decode_secret(const void* data, size_t size);

typedef std::vector<std::string> str_list_t;
str_list_t decode_server_list(const std::string& i33628_txt_str);
#endif  // DECODE_SECRET_H
