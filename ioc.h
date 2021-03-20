#ifndef IOC_H
#define IOC_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

boost::asio::io_context& ioc_get();
boost::asio::ip::tcp::resolver& resolver_get();

#endif  // IOC_H
