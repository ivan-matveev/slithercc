#include "websocket_boost.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/beast/websocket/detail/hybi13.hpp>
#include <boost/beast/websocket.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <future>

#include <zlib.h>

#include "connect.h"
#include "uri.h"
#include "log.h"
#include "util.h"
#include "http_get.h"
#include "ioc.h"
#include "decode_secret.h"

#include "packet_to_client.h"
#include "packet_to_server.h"

namespace beast = boost::beast;	 // from <boost/beast.hpp>
namespace http = beast::http;	   // from <boost/beast/http.hpp>
namespace net = boost::asio;		// from <boost/asio.hpp>
namespace detail = boost::beast::websocket::detail;
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
using tcp = boost::asio::ip::tcp;	   // from <boost/asio/ip/tcp.hpp>

std::vector<char> res_body_decode(const http::response<http::string_body>& res)
{
	LOGCC << res.base();
	boost::iostreams::array_source src( res.body().data(), res.body().size() );
	boost::iostreams::filtering_stream<boost::iostreams::input> is;
	LOGCC << "decompressing " << res["Content-Encoding"]
		<< " " << res["Transfer-Encoding"] << std::endl;
	if (res["Content-Encoding"] == "deflate")
		is.push(boost::iostreams::zlib_decompressor{ -MAX_WBITS });
	else if (res["Content-Encoding"] == "gzip")
		is.push(boost::iostreams::gzip_decompressor{});
	else if (res["Content-Encoding"] == "")
	{
	}
// 	if (res["Transfer-Encoding"] == "chunked")
	is.push(src);
	std::vector<char> out_vect(4096);
	boost::iostreams::array_sink out(&out_vect[0], &out_vect[0] + out_vect.size());
	boost::iostreams::copy(is, out);
	return out_vect;
}

void res_print(const http::response<http::string_body>& res)
{
#ifdef DEBUG_LOG
	LOGCC << res.base();
	boost::iostreams::array_source src( res.body().data(), res.body().size() );
	boost::iostreams::filtering_stream<boost::iostreams::input> is;
	LOGCC << "decompressing " << res["Content-Encoding"]
		<< " " << res["Transfer-Encoding"] << std::endl;
	if (res["Content-Encoding"] == "deflate")
		is.push(boost::iostreams::zlib_decompressor{ -MAX_WBITS });
	else if (res["Content-Encoding"] == "gzip")
		is.push(boost::iostreams::gzip_decompressor{});
	else if (res["Content-Encoding"] == "")
	{
	}
// 	if (res["Transfer-Encoding"] == "chunked")
	is.push(src);
	boost::iostreams::copy(is, std::cout);
#else
	(void)res;
#endif  //  DEBUG_LOG
}

std::string set_cookie_split(const std::string set_cookie)
{
	str_list_t cook_lst = split_str(set_cookie, ";");
	if (cook_lst.size() == 0)
		return std::string("");
	return *cook_lst.begin();
}

std::string connect_html()
{
	std::string i33628_txt_str;

	const char* get_list[] = {
		"/i33628.txt"
	};

	// this shall redirect to slither.io
	http_get_res_t res1 = http_get({"www.slither.io", "80", "/"});
	res_print(res1.res);
	assert(res1.res.base().result_int() == 302);

	std::string cookie;
	for (size_t idx = 0; idx < sizeof(get_list) / sizeof(*get_list); ++idx)
	{
		const char* target = get_list[idx];
		LOGCC << target << "\n";
		host_port_target_t hpt{"slither.io", "80", target};
		http_get_res_t res = http_get(hpt, cookie);
		if (res.res.base().result_int() == 302)
		{
			std::string uri_str = res.res.base()["Location"].to_string();
			LOGCC << "redirect to: " << uri_str << "\n";
			Uri uri = Uri::Parse(uri_str);
			host_port_target_t hpt_redir{hpt};
			hpt.host = uri.Host;
			res = http_get(hpt_redir, cookie);
			continue;
		}
		auto const it = res.res.find("Set-Cookie");
		if(it != res.res.end())
		{
			std::string set_cookie(it->value());
			cookie = set_cookie_split(set_cookie);
			LOG("----------cookie:%s", cookie.c_str());

		}
		if (0 == strcmp(target, "/i33628.txt"))
		{
			LOG("----------/i33628.txt");
			std::vector<char> i33628_txt = res_body_decode(res.res);
			i33628_txt_str.assign(i33628_txt.begin(), i33628_txt.end());
			LOG("%s\n", i33628_txt_str.c_str());
		}
	}
	return i33628_txt_str;
}

void ws_handshake_req_modify(boost::beast::websocket::request_type& req)
{
	// I am FF. Trust me.
//		req.set("Host", host_port_str);
	req.set("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0");
	req.set("Accept", "*/*");
	req.set("Accept-Language", "en-US,en;q=0.5");
	req.set("Accept-Encoding", "gzip, deflate");
	req.set("Sec-WebSocket-Version", "13");
	req.set("Origin", "http://slither.io");
	req.set("Sec-WebSocket-Extensions", "permessage-deflate");
	auto const key = req["Sec-WebSocket-Key"];
	req.set("Sec-WebSocket-Key", key);
	req.set("Connection", "keep-alive, Upgrade");
	req.set("Pragma", "no-cache");
	req.set("Cache-Control", "no-cache");
	req.set("Upgrade", "websocket");
	LOGCC << "\n" << req << "\n";
}

bool connect_ws_ptc(websocket::stream<tcp::socket>& ws, const char* host_port_str)
{
	str_list_t host_port = split_str(host_port_str, ":");
	std::string host = host_port[0];
	std::string port = "80";
	const char* target = "/ptc";
	tcp::resolver& resolver = resolver_get();
	auto const results = resolver.resolve(host, port);
	net::connect(ws.next_layer(), results.begin(), results.end());
	http::response<http::string_body> res;
	try
	{
		ws.handshake_ex(res, host, target, ws_handshake_req_modify);
	}
	catch(std::exception const& e)
	{
		ERRCC << "Error: " << e.what() << std::endl;
		ERRCC << "\n" << res << "\n";
		exit(EXIT_FAILURE);
		return false;
	}
	LOGCC << "\n" << res << "\n";
	ws.binary(true);
	ws_write(ws, "p", 1);
	ws_buf_t buf{};
	size_t size = ws_read(ws, buf);
	LOG("size:%zu", size);
	if (size == 0)
		ERR("size == 0");
	if (buf[0] != 'p')
	{
		ERR("buf[0] != 'p'");
		return false;
	}
	return true;
}

bool connect_ws(
	websocket::stream<tcp::socket>& ws, const char* host_port_str,
	const skin_config_t& skin_config, bool test_server)
{
	str_list_t host_port = split_str(host_port_str, ":");
	std::string host = host_port[0];
	std::string port = "80";
	if (host_port.size() > 1)
		port = host_port[1];
	const char* target = "/slither";
	tcp::resolver& resolver = resolver_get();
	auto const results = resolver.resolve(host, port);
	net::connect(ws.next_layer(), results.begin(), results.end());
	http::response<http::string_body> res;
	try
	{
		ws.handshake_ex(res, host, target, ws_handshake_req_modify);
	}
	catch(std::exception const& e)
	{
		ERRCC << "Error: " << e.what() << std::endl;
		ERRCC << "\n" << res << "\n";
		exit(EXIT_FAILURE);
		return false;
	}
	LOGCC << "\n" << res << "\n";
	ws.binary(true);

	ws_write(ws, "c", 1);
	if (test_server)
		return true;
	ws_buf_t resp_6{};
	ws_read(ws, resp_6);
	assert(resp_6[2] == '6');
	secret_result_t secret_result = decode_secret(resp_6.data(), 165);
	ws_write(ws, secret_result.data(), 24);

	ws_buf_t buf{};
	pkt_skin_t* pkt_skin = reinterpret_cast<pkt_skin_t*>(buf.data());
	pkt_skin->pkt_type = 's';
	pkt_skin->protocol_id = 10;
	pkt_skin->skin_id = skin_config.skin_id;
	pkt_skin->nickname_len = strlen(skin_config.nickname);
	memcpy(&buf[4], skin_config.nickname, pkt_skin->nickname_len);
	ws_write(ws, buf, 21);

	return true;
}

websocket::stream<tcp::socket>
connect_ws(const char* host_port_str, const skin_config_t& skin_config, bool test_server)
{
	tcp::resolver& resolver_get();
	static websocket::stream<tcp::socket> ws_ptc{ioc_get()};
	if (!test_server)
		assert(connect_ws_ptc(ws_ptc, host_port_str));
	websocket::stream<tcp::socket> ws{ioc_get()};
	bool ok = connect_ws(ws, host_port_str, skin_config, test_server);
	if (!ok)
		ERR("failed:%s", host_port_str);

	return ws;
}
