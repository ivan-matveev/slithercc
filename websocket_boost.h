#ifndef WEBSOCKET_BOOST_H
#define WEBSOCKET_BOOST_H

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
#include <boost/iostreams/copy.hpp>
#include <boost/beast/websocket/detail/hybi13.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/config.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/ostream.hpp>

namespace beast = boost::beast;	 // from <boost/beast.hpp>
namespace http = beast::http;	   // from <boost/beast/http.hpp>
namespace net = boost::asio;		// from <boost/asio.hpp>
namespace detail = boost::beast::websocket::detail;
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
using tcp = boost::asio::ip::tcp;	   // from <boost/asio/ip/tcp.hpp>

typedef std::array<uint8_t, 1024> ws_buf_t;

bool connect_ws(tcp::resolver& resolver, websocket::stream<tcp::socket>& ws, const char* host_port_str);
websocket::stream<tcp::socket>
	connect_ws(net::io_context& ioc,  const char* host_port_str);

size_t ws_read(websocket::stream<tcp::socket>& ws, boost::beast::multi_buffer& buffer);
size_t ws_read(websocket::stream<tcp::socket>& ws, ws_buf_t& buf);
size_t ws_write(websocket::stream<tcp::socket>& ws, const ws_buf_t& buf, size_t size);
size_t ws_write(websocket::stream<tcp::socket>& ws, const void* buf, size_t size);
bool ws_close(websocket::stream<tcp::socket>& ws);
#endif  // WEBSOCKET_BOOST_H
