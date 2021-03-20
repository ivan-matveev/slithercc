#include "websocket_boost.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <deque>
#include <vector>

#include "packet_to_client.h"
#include "game_rec.h"
#include "clock.h"
#include "log.h"
#include "util.h"

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

typedef std::vector<uint8_t> pkt_data_t;

struct pkt_t
{
	game_evt_rec_hdr_t hdr;
	pkt_data_t data;
};

typedef std::vector<pkt_t> pkt_vec_t;
typedef std::deque<pkt_t> pkt_queue_t;

FILE* game_evt_rec_fh;

// Adjust settings on the stream
template<class NextLayer>
void
setup_stream(websocket::stream<NextLayer>& ws)
{
    // These values are tuned for Autobahn|Testsuite, and
    // should also be generally helpful for increased performance.
    websocket::permessage_deflate pmd;
    pmd.client_enable = true;
    pmd.server_enable = true;
    pmd.compLevel = 3;
    ws.set_option(pmd);
    ws.auto_fragment(false);
    ws.read_message_max(64 * 1024 * 1024);
}

pkt_queue_t read_rec_file(const char* file_name)
{
	FILE* fh = fopen(file_name, "r");
	assert(fh != nullptr);
	pkt_queue_t pkt_queue;
	uint64_t rec_start_us = 0;
	for(;;)
	{
		ssize_t size = 0;
		uint8_t buf[1024];
		size = fread(buf, 1, sizeof(game_evt_rec_hdr_t), fh);
		if (size < static_cast<ssize_t>(sizeof(game_evt_rec_hdr_t)))
			break;
		const game_evt_rec_hdr_t rec_hdr = *(reinterpret_cast<game_evt_rec_hdr_t*>(buf));
		size = fread(buf, 1, rec_hdr.size, fh);
		if (size < static_cast<ssize_t>(sizeof(pkt_hdr_t)))
			break;
		pkt_t pkt{rec_hdr, pkt_data_t(buf, buf + rec_hdr.size)};

		if (rec_start_us == 0)
			rec_start_us = pkt.hdr.tstamp;
		pkt.hdr.tstamp -= rec_start_us;
// 		LOG("size:%2jd tstamp:%jd", rec_hdr.size, rec_hdr.tstamp);

		pkt_queue.emplace_back(pkt);
	}
	fclose(fh);
	return pkt_queue;
}

std::vector<uint8_t> rec_data;

void do_session1(tcp::socket& socket, const pkt_queue_t& pkt_queue_all)
{
    try
    {
        websocket::stream<tcp::socket> ws{std::move(socket)};
        ws.accept();
        ws.binary(true);

		uint64_t rec_us = 0;
		uint64_t rec_us_prev = 0;
		uint64_t play_start_us = uptime_us();
		uint64_t rec_start_us = 0;
		std::deque<std::vector<uint8_t>> pkt_queue;
        for(const pkt_t& pkt : pkt_queue_all)
        {
			const game_evt_rec_hdr_t rec_hdr = pkt.hdr;
			rec_us = rec_hdr.tstamp;
			if (rec_us_prev == 0)
				rec_us_prev = rec_us;
			if (rec_start_us == 0)
				rec_start_us = rec_us;
			int64_t wait_until_us = rec_us - rec_start_us + play_start_us;
			int64_t wait_us = wait_until_us - uptime_us();
// 			LOG("rec_us:%jd rec_us_prev:%jd wait_until_us:%jd now:%ju wait_us:%jd",
// 				rec_us, rec_us_prev, wait_until_us, uptime_us(), wait_us);
			LOG("wait_us:%jd", wait_us);
			rec_us_prev = rec_us;
			if (wait_us > 33000)
			{
				while(static_cast<int64_t>(uptime_us()) < wait_until_us){}
				for (auto pkt1 : pkt_queue)
				{
					boost::asio::const_buffer buffer(pkt1.data(), pkt1.size());
					ws.write(buffer);
				}
				pkt_queue.clear();
				boost::asio::const_buffer buffer(pkt.data.data(), pkt.data.size());
				ws.write(buffer);
			}
			else
				pkt_queue.push_back(pkt.data);
        }
        LOG("end");
    }
    catch(boost::system::system_error const& se)
    {
        // This indicates that the session was closed
        if(se.code() != websocket::error::closed)
            std::cerr << "Error: " << se.code().message() << std::endl;
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

struct config_t
{
	std::string address;
	unsigned short port;
	std::string play_file;
};

config_t parse_opts(int argc, const char* argv[])
{
	config_t config{};
	config.address = "127.0.0.1";
	config.port = 8080;
	for (ssize_t idx = 1; idx < argc; ++idx)
	{
		std::string opt = argv[idx];
		if (!opt.find('='))
			continue;
		key_val_t key_val = key_val_split(opt, "=");
		if (key_val.key == "address_port")
		{
			std::string address_port_str = key_val.val;
			key_val_t address_port_key_val = key_val_split(address_port_str, ":");
			config.address = strtol(address_port_key_val.key.c_str(), NULL, 10);
			config.port = strtol(address_port_key_val.val.c_str(), NULL, 10);
		}
		else if (key_val.key == "play_file")
			config.play_file = key_val.val;
	}
	return config;
}

int main(int argc, const char* argv[])
{
	run_time_us();
	config_t config = parse_opts(argc, argv);
	if (config.play_file.length() == 0)
	{
		ERR("please specify play_file=[file]");
		return EXIT_FAILURE;
	}
	pkt_queue_t pkt_queue = read_rec_file(config.play_file.c_str());
	LOG("pkt_queue.size():%zu", pkt_queue.size());

    try
    {
        auto const address = boost::asio::ip::make_address(config.address.c_str());
        auto const port = config.port;

        boost::asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {address, port}};
        for(;;)
        {
            tcp::socket socket{ioc};
            acceptor.accept(socket);  // Block until we get a connection
			do_session1(socket, pkt_queue);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
