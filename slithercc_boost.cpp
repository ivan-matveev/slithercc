#include "websocket_boost.h"

#include "packet_to_client.h"
#include "packet_to_server.h"
#include "connect.h"
#include "game.h"
#include "game_rec.h"
#include "log.h"
#include "ioc.h"
#include "timerfd_grid.h"
#include "decode_secret.h"
#include "dumb_profile.h"

#include <cstdlib>
#include <csignal>
#include <cmath>
#include <string>
#include <chrono>
#include <thread>
#include <assert.h>


sig_atomic_t run = 1;

void signal_handler(int sig_num, siginfo_t *info, void *ucontext)
{
	(void)sig_num;
	(void)info;
	(void)ucontext;
	run = 0;
}

void signal_handler_register()
{
	struct sigaction sigact = {};
	sigact.sa_sigaction = signal_handler;
	if (0 != sigaction(SIGINT, &sigact, NULL))
		ERR("failed: sigaction()");
}

FILE* game_evt_rec_fh;
void game_evt_rec(const void* data, size_t size)
{
	if (game_evt_rec_fh == nullptr)
		return;
	game_evt_rec_hdr_t hdr{uptime_us(), size};
	fwrite(&hdr, sizeof(hdr), 1, game_evt_rec_fh);
	fwrite(data, size, 1, game_evt_rec_fh);
}

typedef std::vector<uint8_t> pkt_vec_t;
std::deque<pkt_vec_t> pkt_queue;
std::mutex pkt_queue_lock;

std::mutex rw_lock;

void read_thread_func(websocket::stream<tcp::socket>& ws)
{
	while(run)
	{
		{
			std::lock_guard<std::mutex> lock_guard(rw_lock);
			if (!ws.is_open())
			{
				run = false;
				break;
			}
			boost::beast::multi_buffer buffer;
			ws_read(ws, buffer);
			for (const boost::asio::const_buffer& sbuf : buffer.data())
			{
				if (sbuf.size() < sizeof(pkt_hdr_t))
					continue;
				const uint8_t* data_begin = static_cast<const uint8_t*>(sbuf.data());
				const uint8_t* data_end = static_cast<const uint8_t*>(sbuf.data()) + sbuf.size();
				{
					std::lock_guard<std::mutex> lock_guard(pkt_queue_lock);
					pkt_queue.emplace_back(pkt_vec_t(data_begin, data_end));
				}
				game_evt_rec(sbuf.data(), sbuf.size());
			}
		}
	}
}

void write_thread_func(websocket::stream<tcp::socket>& ws, game_t& game)
{
	while(run)
	{
		if (!timerfd_grid_us_wait<200000>())
		{
			LOG("fail: timerfd_grid_us_wait<>()");
		}
		if (!game.ready())
			continue;
		{
			std::lock_guard<std::mutex> lock_guard(rw_lock);
			if (!ws.is_open())
				break;
			game.pkt_send(
				[&ws](const char pkt) mutable { ws_write(ws, &pkt, sizeof(pkt)); }
			);
		}
	}
}

void play_rec()
{
	FILE* fh = fopen("slithercc.rec", "r");
	assert(fh != nullptr);
	uint64_t rec_us = 0;
	uint64_t rec_us_prev = 0;
	ssize_t size = 0;
	uint8_t buf[1024];
	while(run)
	{
		size = fread(buf, 1, sizeof(game_evt_rec_hdr_t), fh);
		if (size < static_cast<ssize_t>(sizeof(game_evt_rec_hdr_t)))
			break;
		const game_evt_rec_hdr_t rec_hdr = *(reinterpret_cast<game_evt_rec_hdr_t*>(buf));
		size = fread(buf, 1, rec_hdr.size, fh);
		if (size < static_cast<ssize_t>(sizeof(pkt_hdr_t)))
			break;
		rec_us = rec_hdr.tstamp;
		if (rec_us_prev == 0)
			rec_us_prev = rec_us;
		int64_t wait_us = rec_us - rec_us_prev;
		std::this_thread::sleep_for(std::chrono::microseconds(wait_us));
		{
			std::lock_guard<std::mutex> lock_guard(pkt_queue_lock);
			pkt_queue.emplace_back(pkt_vec_t(buf, buf + rec_hdr.size));
		}
		rec_us_prev = rec_us;
	}
	fclose(fh);
	run = false;
}

template<class NextLayer>
void setup_stream(websocket::stream<NextLayer>& ws)
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

struct config_t
{
	std::string server;
	std::string test_server;
	std::string nickname;
	int skin_id;
	std::string record_file;
	std::string play_file;
	xy_t window_size;
	bool show_usage;
};

void usage()
{
	std::cout
	<< "\n"
	<< "slither.io client\n"
	<< "\n"
	<< "The snake follows mouse pointer. Hold left mouse button to accelerate.\n"
	<< "Eat color dots and flying prey. If you hit other snake you die.\n"
	<< "\n"
	<< "Usage examples:\n"
	<< "\n"
	<< "./slithercc\n"
	<< "\n"
	<< "./slithercc nickname=slithercc skin_id=5 window_size=1200x800 "
	<< "server=145.239.6.19:444\n"
	<< "\n"
	<< "With no options slithercc connects to the first server in the list"
	<< "downloaded from slither.io.\n"
	<< "The list is saved to slithercc.server_list file\n"
	<< "\n"
	<< "options: \"h\" or \"help\" print this message\n"
	<< "\n"
	<< "See readme.adoc for full list of options\n"
	<< "\n";
}

config_t parse_opts(int argc, const char* argv[])
{
	config_t config{};
	for (ssize_t idx = 1; idx < argc; ++idx)
	{
		std::string opt = argv[idx];
		if (!opt.find('='))
			continue;
		key_val_t key_val = key_val_split(opt, "=");
		if (key_val.key == "server")
			config.server = key_val.val;
		else if (key_val.key == "test_server")
			config.test_server = key_val.val;
		else if (key_val.key == "nickname")
			config.nickname = key_val.val;
		else if (key_val.key == "skin_id")
			config.skin_id = strtol(key_val.val.c_str(), NULL, 10);
		else if (key_val.key == "record_file")
			config.record_file = key_val.val;
		else if (key_val.key == "play_file")
			config.play_file = key_val.val;
		else if (key_val.key == "window_size")
		{
			std::string win_sz_str = key_val.val;
			key_val_t win_sz_key_val = key_val_split(win_sz_str, "x");
			config.window_size.x = strtol(win_sz_key_val.key.c_str(), NULL, 10);
			config.window_size.y = strtol(win_sz_key_val.val.c_str(), NULL, 10);
		}
		else if (key_val.key == "-h" || key_val.key == "h")
			config.show_usage = true;
		else if (key_val.key == "--help" || key_val.key == "help")
			config.show_usage = true;
	}
	return config;
}

int main(int argc, const char* argv[])
{
	run_time_us();
	config_t config = parse_opts(argc, argv);
	if (config.show_usage)
	{
		usage();
		return EXIT_SUCCESS;
	}
	std::string font_path = realpath_str(std::string(argv[0]));
	font_path = dirname(font_path) + "/Arimo-Regular.ttf";
	screen_sdl_t screen(config.window_size.x, config.window_size.y, font_path);
	screen.window_title("slithercc");
	game_t* game_p = new game_t(screen);
	if (!game_p)
		return EXIT_FAILURE;
	game_t& game = *game_p;
	bool play_file = config.play_file.length() > 0 ? true : false;
	bool test_server = config.test_server.length() > 0 ? true : false;
	skin_config_t skin_config{config.skin_id, config.nickname.c_str()};
	websocket::stream<tcp::socket> ws{ioc_get()};
	std::thread read_tread;
	if (play_file)
	{
		read_tread = std::thread(play_rec);
	}
	else if (test_server)
	{
		ws = connect_ws(config.test_server.c_str(), skin_config, test_server);
		assert(ws.is_open());
		setup_stream(ws);
		read_tread = std::thread(read_thread_func, std::ref(ws));
	}
	else
	{
		std::string i33628_txt_str;
		i33628_txt_str = connect_html();
		assert(i33628_txt_str.size() > 0);
		str_list_t server_list = decode_server_list(i33628_txt_str);
		std::string server_list_str;
		for (auto server : server_list)
			server_list_str += server + "\n";
		file_write_str("slithercc.server_list", server_list_str);
		if (config.server.length() == 0)
			config.server = server_list[0];
		ws = connect_ws(config.server.c_str(), skin_config);
		assert(ws.is_open());
		setup_stream(ws);
		if (!play_file && config.record_file.length() > 0)
			game_evt_rec_fh = fopen(config.record_file.c_str(), "w");
		read_tread = std::thread(read_thread_func, std::ref(ws));
	}

	signal_handler_register();

// 	std::thread write_tread(write_thread_func, std::ref(ws), std::ref(game));

	while (run)
	{
// 		DPROFILE;
		ssize_t pkt_cnt;
		{
			std::lock_guard<std::mutex> lock_guard(pkt_queue_lock);
			pkt_cnt = pkt_queue.size();
		}
		for (; pkt_cnt > 0; --pkt_cnt)
		{
			const pkt_vec_t& pkt = pkt_queue.front();
			game.pkt_handle(pkt.data(), pkt.size());
			{
				std::lock_guard<std::mutex> lock_guard(pkt_queue_lock);
				pkt_queue.pop_front();
			}
		}
		if (!game.ready())
			continue;
		timerfd_grid_us_wait<draw_period_us>();
		game.draw();
		if (play_file)
			continue;
		if (!ws.is_open())
		{
			run = false;
			break;
		}
		game.pkt_send(
			[&ws](const char pkt) mutable { ws_write(ws, &pkt, sizeof(pkt)); }
		);
		if (screen.quit)
			run = false;
	}
	read_tread.join();
// 	write_tread.join();

	if (!screen.quit)
	{
		screen.text((screen.width / 2) - 75, screen.height / 2,	screen_sdl_t::color_t{255, 255, 255}, 15,
			"Game over, press any key");
		screen.present();
		screen.wait_any_key();
	}
	ws_close(ws);
	delete game_p;
	if (game_evt_rec_fh != nullptr)
		fclose(game_evt_rec_fh);
	return EXIT_SUCCESS;
}
