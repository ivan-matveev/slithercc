#ifndef HTTP_GET_H
#define HTTP_GET_H

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

// TODO use https://github.com/reBass/uri
struct host_port_target_t
{
	std::string host;
	std::string port;
	std::string target;
};

struct http_get_res_t
{
	host_port_target_t hpt;
	boost::beast::http::response<boost::beast::http::string_body> res;
};

http_get_res_t http_get(host_port_target_t hpt, std::string cookie = "");

#endif  // HTTP_GET_H
