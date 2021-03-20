#ifndef CONNECT_H
#define CONNECT_H

#include "util.h"

struct skin_config_t
{
	uint8_t skin_id; // 0-38
	const char* nickname;
};

std::string connect_html(/*tcp::resolver& resolver, tcp::socket& socket*/);
bool connect_ws(websocket::stream<tcp::socket>& ws, const char* host_port_str,
	const skin_config_t& skin_config);
websocket::stream<tcp::socket>
	connect_ws(const char* host_port_str, const skin_config_t& skin_config,
		bool test_server = false);

str_list_t server_list_from_slither_io(const std::string& i33628_txt_str);

#endif  // CONNECT_H
