#include "websocket_boost.h"
#include "packet_to_client.h"
#include "connect.h"
#include "log.h"

// size_t ws_read(websocket::stream<tcp::socket>& ws, ws_buf_t& buf)
// {
// 	beast::flat_buffer buffer;
// 	try
// 	{
// 		ws.read(buffer);
// 	}
// 	catch(std::exception const& e)
// 	{
// 		ERRCC << ":Error: " << e.what() << std::endl;
// 		return 0;
// 	}
// 	boost::asio::const_buffer const_buffer = beast::buffers_front(buffer.data());
// 	size_t size = const_buffer.size();
// 	assert(buf.size() >= size);
// 	size = std::min(buf.size(), size);
// 	memcpy(buf.data(), const_buffer.data(), size);
// 	return size;
// }

// TODO find a way to avoid buffer allocation
#include <boost/beast/core/flat_static_buffer.hpp>
size_t ws_read(websocket::stream<tcp::socket>& ws, ws_buf_t& buf)
{
	boost::beast::flat_static_buffer<buf.size()> buffer;
	size_t size = 0;
	try
	{
		size = ws.read(buffer);
	}
	catch(std::exception const& e)
	{
		ERRCC << ":Error: " << e.what() << std::endl;
		return 0;
	}
	memcpy(buf.data(), buffer.data().data(), size);
	return size;
}

size_t ws_read(websocket::stream<tcp::socket>& ws, boost::beast::multi_buffer& buffer)
{
	size_t size = 0;
	try
	{
		size = ws.read(buffer);
	}
	catch(std::exception const& e)
	{
		ERRCC << ":Error: " << e.what() << std::endl;
		return 0;
	}
	return size;
}

// size_t ws_write(websocket::stream<tcp::socket>& ws, boost::beast::multi_buffer& buffer)
// {
// 	size_t size = 0;
// 	try
// 	{
// 		size = ws.write(buffer);
// 	}
// 	catch(std::exception const& e)
// 	{
// 		ERRCC << ":Error: " << e.what() << std::endl;
// 		return 0;
// 	}
// 	return size;
// }

size_t ws_write(websocket::stream<tcp::socket>& ws, const ws_buf_t& buf, size_t size)
{
	boost::asio::const_buffer buffer(buf.data(), size);
	try
	{
		ws.write(buffer);
	}
	catch(std::exception const& e)
	{
		ERRCC << ":Error: " << e.what() << std::endl;
		return 0;
	}
	return size;
}

size_t ws_write(websocket::stream<tcp::socket>& ws, const void* buf, size_t size)
{
	boost::asio::const_buffer buffer(buf, size);
	try
	{
		ws.write(buffer);
	}
	catch(std::exception const& e)
	{
		ERRCC << ":Error: " << e.what() << std::endl;
		return 0;
	}
	return size;
}

bool ws_close(websocket::stream<tcp::socket>& ws)
{
	if (!ws.is_open())
		return true;
 	beast::error_code ec;
	try
	{
		ws.close(beast::websocket::close_code::normal, ec);
	}
	catch(std::exception const& e)
	{
		ERRCC << ":Error: " << e.what() << std::endl;
		return 0;
	}
// 	assert(!ec || ec == beast::websocket::error::closed);
// 	LOG("", ec);
	return true;
}
