#include "ioc.h"

boost::asio::io_context& ioc_get()
{
	static boost::asio::io_context ioc;
	return ioc;
}

boost::asio::ip::tcp::resolver& resolver_get()
{
	static boost::asio::ip::tcp::resolver resolver{ioc_get()};
	return resolver;
}
