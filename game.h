#ifndef GAME_H
#define GAME_H

#include <vector>
#include <deque>
#include <mutex>
#include <limits>
#include <algorithm>
#include <cmath>
#include <functional>

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "geometry.h"
#include "clock.h"
#include "log.h"
#include "screen_sdl.h"

static const coordinate_t game_radius = 21600;
static const coordinate_t sector_size = 300;
static const size_t snake_min_length = 0; //2;
static const size_t snake_max_part_count = 411;  // mscps
static const coordinate_t snake_step_distance = 42;
static const coordinate_t snake_speed = 185 / snake_step_distance;  // step per second
static constexpr float snake_angular_speed = 4.125f;  // radian in second (convert:  1000ms/8ms * ang[rad])
static const size_t draw_fps = 60;
static const size_t draw_period_us = 1000000 / draw_fps;
static const size_t mouse_period_us = 300000;//250000;
static const size_t ping_period_us = 250000;

struct food_t
{
	coordinate_t x;
	coordinate_t y;
	int color;
	coordinate_t size;
	bool eaten;
	bool operator == (const xy_t& other)
	{
		return x == other.x && y == other.y;
	}

	template <typename Txy>
	bool operator == (const Txy& other)
	{
		return x == other.x && y == other.y;
	}
};

enum rot_dir_t
{
	rot_dir_no,
	rot_dir_ccw,
	rot_dir_cw
};

struct prey_t
{
	uint64_t tstamp_data;
	size_t id;
	xy_t xy;
	xy_t xy_prev;  // for debug
	int color;
	coordinate_t size;
	float rot_angle;
	float rot_wangle;
	float speed;
	rot_dir_t dir;
	bool eaten;
};

static const float invalid_angle = -1.;

struct snake_t
{
	size_t snake_length;
	uint64_t tstamp_data;
	uint64_t tstamp_draw;
	rot_dir_t rot_dir;
	bool dead;
	uint8_t skin;
	std::deque<xy_t> part_list;
	xy_t head;
	xy_t prev;
	float rot_angle;
	float rot_wangle;
	float speed;
	float fam;
	char name[16];

	void clear()
	{
		part_list.clear();
		*this = {};
		rot_dir = rot_dir_no;
		rot_angle = invalid_angle;
		rot_wangle = invalid_angle;
		speed = snake_speed / 1000;  // pixels per millisecond
		memset(name, 0, sizeof(name));
	}

	void move(const xy_t& xy)
	{
		uint64_t now_us = uptime_us();
		prev = part_list.front();
		part_list.emplace_front(xy);
		LOG("dist:%d %jd", distance(xy, part_list[0]), now_us - tstamp_data);
		tstamp_data = now_us;
		squeeze();
		part_list_trim();
	}

	void squeeze()
	{
		// TODO take from game.config
		static const float cst = 0.43;  // cst
		if (part_list.size() < 4)
			return;
		float w = 0.;
		for (size_t idx = 3; idx < part_list.size(); ++idx)
		{
			if (idx <= 4)
				w = cst * idx / 4.;
			part_list[idx].x += 1. * (part_list[idx - 1].x - part_list[idx].x) * w;
			part_list[idx].y += 1. * (part_list[idx - 1].y - part_list[idx].y) * w;
		}
	}

	void part_list_trim()
	{
		static const size_t save_part_count = 40;
		if (part_list.size() <= snake_length + save_part_count)
			return;
		for (size_t cnt = 0; cnt < part_list.size() - snake_length + save_part_count; ++cnt)
			part_list.pop_back();
	}
};

float snake_heading(const snake_t& snake);

struct leaderboard_player_t
{
	size_t body_part_count;  // sct
	size_t font_color;  // 0 - 8
	float fam; //  : 24;  // value / 16777215 -> fam
	std::array<char, 80> name;
};

struct leaderboard_t
{
	size_t rank;
	size_t rank_in_lb;
	size_t player_count;
	bool have_data;
	std::array<leaderboard_player_t, 10> player_list;
};

struct score_t
{
	// allmost copy/paste http://slither.io/s/game832434.js:setMscps()

	int get(size_t sct, float fam)
	{
		if (fpsls.size() <= sct)
			return 0;
		if (fmlts.size() <= sct)
			return 0;
		return std::floor(15. * (fpsls[sct] + fam / fmlts[sct] - 1) - 5) / 1;
	}

	void set_mscps(size_t mscps_)
	{
	    if (mscps_ == mscps)
			return;

        mscps = mscps_;
        fmlts.clear();
        fpsls.clear();
        for (size_t b = 0; b <= mscps; b++)
        {
            if (b >= mscps)
				fmlts.push_back(fmlts[b - 1]);
			else
				fmlts.push_back(std::pow(1. - (1. * b / mscps), 2.25));
            if (0 == b)
				fpsls.push_back(0);
			else
				fpsls.push_back(fpsls[b - 1] + (1. / fmlts[b - 1]));
        }
        float c = fmlts[fmlts.size() - 1];
        float e = fpsls[fpsls.size() - 1];
        for (size_t b = 0; 2048 > b; b++)
        {
            fmlts.push_back(c);
            fpsls.push_back(e);
        }
	}

	size_t mscps;
	std::vector<float> fmlts;
	std::vector<float> fpsls;
};

struct draw_ctx_t
{
	rect_t sect_rect;  // upper left and lower right of all sectors
	coordinate_t width;
	coordinate_t height;
	xy_t game_view_center;
	rect_t game_view_rect;
	float scale;

	bool ready(){ return game_view_center != xy_t{0, 0}; }
};

struct ping_ctx_t
{
	bool wait_pong;
	uint64_t ping_tstamp;
	int32_t ping_pong_us;
	int32_t ping_pong_avg_us;
};

typedef std::function<void (const char)> pkt_sender_t;

struct game_t
{
	game_t(screen_sdl_t& screen_);
	~game_t();
	struct config_t
	{
		coordinate_t game_radius;
		size_t mscps; // maximum snake length in body parts units
		coordinate_t sector_size;  // 300 pix
		float spangdv;  // (value / 10) (coef. to calculate angular speed change depending snake speed) 	4.8 	4.8
		float nsp1;  // (value / 100) (Maybe nsp stands for "node speed"?) 	4.25 	5.39
		float nsp2;  // nsp2 (value / 100) 	0.5 	0.4
		float nsp3;  // nsp3 (value / 100) 	12 	14
		float mamu;  // (value / 1E3) (basic snake angular speed) 	0.033 	0.033
		float manu2;  // (value / 1E3) (angle in rad per 8ms at which prey can turn) 	0.028 	0.028
		float cst;  // (value / 1E3) (snake tail speed ratio ) 	0.43 	0.43
		uint8_t protocol_version;
	};

	typedef void (game_t::*pkt_handler_t)(const uint8_t* buf, size_t size);
	void pkt_handle_init();
	void pkt_handle(const uint8_t* buf, size_t size);
	void pkt_send(pkt_sender_t sender);
	void pkt_init(const uint8_t* buf, size_t size);  // = 'a',  // Initial setup
	void pkt_snake_fam(const uint8_t* buf, size_t size);  //  = 'h',	 // Update snake last body part fullness (fam)
	void pkt_snake_mov(const uint8_t* buf, size_t size);  //  = 'g',			  // Move snake
	void pkt_snake_mov_G(const uint8_t* buf, size_t size);  // G
	void pkt_snake_mov_inc(const uint8_t* buf, size_t size);  // N
	void pkt_snake_rot_e(const uint8_t* buf, size_t size); // 'e'  rotation
	void pkt_snake_rot_3(const uint8_t* buf, size_t size); // '3'
	void pkt_snake_rot_4(const uint8_t* buf, size_t size);  // 4
	void pkt_snake_rot_5(const uint8_t* buf, size_t size); // '5'
	void pkt_snake_inc(const uint8_t* buf, size_t size);  //  = 'n',			  // Increase snake
	void pkt_snake_rem_part(const uint8_t* buf, size_t size);  // r
	void pkt_leaderboard(const uint8_t* buf, size_t size);  //  = 'l',	  // Leaderboard
	void pkt_end(const uint8_t* buf, size_t size);  //  = 'v',			  // dead/disconnect packet
	void pkt_sector_add(const uint8_t* buf, size_t size);  //  = 'W',	   // Add Sector
	void pkt_sector_rem(const uint8_t* buf, size_t size);  //  = 'w',	   // Remove Sector
	void pkt_snake(const uint8_t* buf, size_t size);  //  = 's',			// Add/remove Snake
	void pkt_food_set(const uint8_t* buf, size_t size);  //  = 'F',		 // Add Food, Sent when food that existed before enters range.
	void pkt_food_add(const uint8_t* buf, size_t size);  //  = 'f',  // Add Food, Sent when natural food spawns while in range.
	void pkt_food_eat(const uint8_t* buf, size_t size);  //  = 'c',  // Food eaten
	void pkt_prey(const uint8_t* buf, size_t size);  // y
	void pkt_prey_upd(const uint8_t* buf, size_t size);  // j
	void pkt_minimap(const uint8_t* buf, size_t size);  //  = 'u'
//	void pkt_kill(const uint8_t* buf, size_t size);  //  = 'k',	  // Kill (unused in the game-code)
//	void pkt_highscore(const uint8_t* buf, size_t size);  //  = 'm',		// Global highscore
	void pkt_pong(const uint8_t* buf, size_t size);  //  = 'p',			 // Pong

	// custom debug
//	void pkt_reset(const uint8_t* buf, size_t size);  //  = '0',  // reset debug render buffer
//	void pkt_draw(const uint8_t* buf, size_t size);  //  = '!',   // draw something

	bool draw_ctx_update();
	void draw();
	void draw_minimap();
	void draw_leaderboard();
	void draw_background();
	void draw_food();
	void draw_prey(uint64_t now_us);
	void draw_snake_list(uint64_t now_us);
	void draw_snake_prepare1(size_t snake_id, uint64_t now_us);
	void draw_snake(size_t snake_id, uint64_t now_us);
	void draw_part(xy_t xy, size_t radius, size_t snake_id);
	template <typename Txy>
	xy_t screen_xy(const Txy& game_xy);
	template <typename Txy>
	xy_t game_xy(const Txy& screen_xy);
	float mouse_angle();
	bool my_snake_dead();
	void edge_points_calc();
	bool ready(){ return my_snake_id != snake_id_invalid; }
	snake_t& snake_get(size_t snake_id)
	{
		if (snake_list[snake_id] == nullptr)
		{
			snake_list[snake_id] = new snake_t();
			snake_list[snake_id]->clear();
		}
		return *snake_list[snake_id];
	}
	// TODO void snake_rem(size_t snake_id);

public:
	static const size_t snake_id_invalid = std::numeric_limits<size_t>::max();

	config_t config;
	screen_sdl_t& screen;
	draw_ctx_t draw_ctx;
	pkt_handler_t pkt_handler_list[std::numeric_limits<char>::max()];
	bool have_data;
	size_t my_snake_id;
	std::deque<xy_t> sector_list;
	std::deque<food_t> food_list;
	std::array<snake_t*, std::numeric_limits<uint16_t>::max()> snake_list;
	std::deque<size_t> snake_id_list;
	std::deque<prey_t> prey_list;
	std::array<uint8_t, 80 * 80> minimap;
	std::array<xy_t, 360> edge_points;
	leaderboard_t leaderboard;
	score_t score;
	float fps;
	ping_ctx_t ping_ctx;
};
#endif  // GAME_H
