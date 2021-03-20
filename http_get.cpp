#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket/detail/hybi13.hpp>
#include <boost/beast/websocket.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <future>
#include <functional>

#include <zlib.h>
#include <assert.h>

#include "http_get.h"
#include "uri.h"
#include "log.h"
#include "util.h"
#include "ioc.h"

#include "packet_to_client.h"

namespace beast = boost::beast;	 // from <boost/beast.hpp>
namespace http = beast::http;	   // from <boost/beast/http.hpp>
namespace net = boost::asio;		// from <boost/asio.hpp>
namespace detail = boost::beast::websocket::detail;
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
using tcp = boost::asio::ip::tcp;	   // from <boost/asio/ip/tcp.hpp>

std::string http_get_target_media_type(const std::string& target)
{
	if (
		str_ends_with(target, ".png") ||
		str_ends_with(target, ".jpg")
		)
		return "image/webp,*/*";

	if (target == "/")
		return "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
	//js, /ptc, txt
	return "*/*";

}

http_get_res_t http_get(host_port_target_t hpt, std::string cookie)
{
	tcp::resolver resolver{ioc_get()};
	tcp::socket socket{ioc_get()};
	auto const host = hpt.host;
	auto const port = hpt.port;
	auto const target = hpt.target;
	int version = 11;

	http::request<http::string_body> req{http::verb::get, target, version};
	req.set("Host", host);
	req.set("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0");
	req.set("Accept", http_get_target_media_type(target));
	req.set("Accept-Language", "en-US,en;q=0.5");
	req.set("Accept-Encoding", "gzip, deflate");
	if (target != "/")
		req.set("Referer", "http://slither.io/");
	req.set("Connection", "keep-alive");
// 	if (target == "/")
// 		req.set("Upgrade-Insecure-Requests", "1");
	if (cookie.length() > 0)
		req.set("Cookie", cookie);

	beast::flat_buffer buffer;
	http::response<http::string_body> res;
	try
	{
		auto const results = resolver.resolve(host, port);
		net::connect(socket, results.begin(), results.end());
		LOGCC << "\n------------------request:\n" << req ;
		http::write(socket, req);
		http::read(socket, buffer, res);
	}
	catch(std::exception const& e)
	{
		ERRCC << "Error: " << e.what() << std::endl;
		return http_get_res_t{{hpt}, {}};
	}

	return http_get_res_t{{hpt}, {res}};
}
