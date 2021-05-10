#include <endian.h>
#include <assert.h>

#include <algorithm>
#include <limits>
#include <thread>

#include <cmath>

#include "game.h"
#include "packet_to_client.h"
#include "packet_to_server.h"
#include "log.h"
#include "dumb_profile.h"

game_t::game_t(screen_sdl_t& screen_)
: config()
, screen(screen_)
, draw_ctx()
, pkt_handler_list()
, have_data(false)
, my_snake_id(game_t::snake_id_invalid)
, sector_list()
, food_list()
, snake_list()
, snake_id_list()
, prey_list()
, minimap()
, edge_points()
, leaderboard()
, score()
, fps(0)
, ping_ctx()
{
	pkt_handle_init();
	config.game_radius = game_radius;
	config.sector_size = sector_size;
	config.mscps = snake_max_part_count;
	edge_points_calc();
	score.set_mscps(config.mscps);
}

game_t::~game_t()
{
	LOG("");
	for (snake_t* snake_p : snake_list)
	{
		if (snake_p != nullptr)
		{
			delete snake_p;
			snake_p = nullptr;
		}
	}
}

void game_t::pkt_handle_init()
{
	pkt_handler_list['a'] = &game_t::pkt_init;
	pkt_handler_list['s'] = &game_t::pkt_snake;
	pkt_handler_list['h'] = &game_t::pkt_snake_fam;
	pkt_handler_list['g'] = &game_t::pkt_snake_mov;
	pkt_handler_list['G'] = &game_t::pkt_snake_mov_G;
	pkt_handler_list['N'] = &game_t::pkt_snake_mov_inc;
	pkt_handler_list['n'] = &game_t::pkt_snake_inc;
	pkt_handler_list['r'] = &game_t::pkt_snake_rem_part;
	pkt_handler_list['3'] = &game_t::pkt_snake_rot_3;
	pkt_handler_list['e'] = &game_t::pkt_snake_rot_e;
	pkt_handler_list['4'] = &game_t::pkt_snake_rot_4;
	pkt_handler_list['5'] = &game_t::pkt_snake_rot_5;
	pkt_handler_list['W'] = &game_t::pkt_sector_add;
	pkt_handler_list['w'] = &game_t::pkt_sector_rem;
	pkt_handler_list['F'] = &game_t::pkt_food_set;
	pkt_handler_list['f'] = &game_t::pkt_food_add;
	pkt_handler_list['b'] = &game_t::pkt_food_add;
	pkt_handler_list['c'] = &game_t::pkt_food_eat;
	pkt_handler_list['y'] = &game_t::pkt_prey;
	pkt_handler_list['j'] = &game_t::pkt_prey_upd;
	pkt_handler_list['u'] = &game_t::pkt_minimap;
	pkt_handler_list['l'] = &game_t::pkt_leaderboard;
	pkt_handler_list['p'] = &game_t::pkt_pong;
	pkt_handler_list['v'] = &game_t::pkt_end;
}

void game_t::pkt_handle(const uint8_t* buf, size_t size)
{
// 	DPROFILE;
	if (size < sizeof(pkt_hdr_t)){ LOG("wrong size:%zu", size); return; }
	pkt_hdr_t pkt_hdr;
	memcpy(&pkt_hdr, buf, sizeof(pkt_hdr));
	const uint8_t pkt_type = static_cast<uint8_t>(pkt_hdr.packet_type);
	buf += sizeof(pkt_hdr_t);
	size -= sizeof(pkt_hdr_t);
	if (pkt_handler_list[pkt_type] == nullptr)
	{
		LOG(" unknown packet:%c", static_cast<char>(pkt_type));
		return;
	}
	(this->*(pkt_handler_list[pkt_type]))(buf, size);
	have_data = true;
}

void game_t::pkt_send(pkt_sender_t sender)
{
	float ang;
	uint8_t btn = 254;

	if (!ready())
		return;

	uint64_t now_us = uptime_us();
	if (now_us - ping_ctx.ping_tstamp >= ping_period_us &&
		!ping_ctx.wait_pong)
	{
		uint8_t ping = 251;
		ping_ctx.ping_tstamp = now_us;
		ping_ctx.wait_pong = true;
		sender(ping);
	}

	ang = mouse_angle();
	if (screen.mouse_button_left())
		btn = 253;
	else
		btn = 254;

	static float ang_prev = 0;
	static uint8_t btn_prev = 0;
	static uint64_t ang_tstamp = 0;
	static uint64_t send_tstamp = 0;
	static const uint64_t send_period_min_us = 100000;

	if (!draw_ctx.ready())
		return;

	if (now_us - send_tstamp < send_period_min_us)
		return;

	if (std::abs(ang - ang_prev) > M_PI * 1 / 360 ||
		now_us - ang_tstamp >= mouse_period_us)
	{
		uint8_t pkt = ang * 251 / (M_PI * 2);
		LOG("ang:%f ang_prev:%f pkt:%u %ju %ju %jd",
			ang, ang_prev, pkt, now_us, ang_tstamp, now_us - send_tstamp);
		sender(pkt);
		ang_tstamp = now_us;
		ang_prev = ang;
	}
	if (btn != btn_prev)
	{
		LOG("btn:%u btn_prev:%u", btn,  btn_prev);
		sender(btn);
		btn_prev = btn;
	}
	send_tstamp = now_us;
}

void game_t::pkt_init(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_init_t)){ LOG("wrong size:%zu", size); return; }
	pkt_init_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	LOG("%u %u %u %u %u %u %u %u %u %u %u %u",
		be24toh(pkt.game_radius),
		be16toh(pkt.mscps),
		be16toh(pkt.sector_size),
		be16toh(pkt.sector_count_along_edge),
		pkt.spangdv,
		be16toh(pkt.nsp1),
		be16toh(pkt.nsp2),
		be16toh(pkt.nsp3),
		be16toh(pkt.mamu),
		be16toh(pkt.manu2),
		be16toh(pkt.cst),
		pkt.protocol_version
		);

	config.game_radius = be24toh(pkt.game_radius);
	config.mscps = be16toh(pkt.mscps); // maximum snake length in body parts units
	config.sector_size = be16toh(pkt.sector_size);
	config.spangdv = 1. * pkt.spangdv / 10;  // (value / 10) (coef. to calculate angular speed change depending snake speed) 	4.8 	4.8
	config.nsp1 = 1. * be16toh(pkt.nsp1) / 100;  // (value / 100) (Maybe nsp stands for "node speed"?) 	4.25 	5.39
	config.nsp2 = 1. * be16toh(pkt.nsp2) / 100;  // nsp2 (value / 100) 	0.5 	0.4
	config.nsp3 = 1. * be16toh(pkt.nsp3) / 100;  // nsp3 (value / 100) 	12 	14
	config.mamu = 1. * be16toh(pkt.mamu) / 1000;  // (value / 1E3) (basic snake angular speed) 	0.033 	0.033
	config.manu2 = 1. * be16toh(pkt.manu2) / 1000;  // (value / 1E3) (angle in rad per 8ms at which prey can turn) 	0.028 	0.028
	config.cst = 1. * be16toh(pkt.cst) / 1000;  // (value / 1E3) (snake tail speed ratio ) 	0.43 	0.43
	config.protocol_version = pkt.protocol_version;

	score.set_mscps(config.mscps);
	if (config.game_radius != game_radius ||
		config.sector_size != sector_size)
		edge_points_calc();
}

// W
void game_t::pkt_sector_add(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_sector_add_t)){ LOG("wrong size:%zu", size); return; }
	pkt_sector_add_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	sector_list.push_back(xy_t{pkt.x, pkt.y});
	LOG(" %u:%u %u:%u", pkt.x, pkt.y, pkt.x * config.sector_size, pkt.y * config.sector_size);
}

// w
void game_t::pkt_sector_rem(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_sector_rem_t)){ LOG("wrong size:%zu", size); return; }
	pkt_sector_rem_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	const xy_t sect_to_rm{pkt.x, pkt.y};
	sector_list.erase(std::remove(sector_list.begin(), sector_list.end(), sect_to_rm));
	LOG(" %u:%u %u:%u", pkt.x, pkt.y, pkt.x * config.sector_size, pkt.y * config.sector_size);
}

// F
void game_t::pkt_food_set(const uint8_t* buf, size_t size)
{
	pkt_food_set_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	const size_t pkt_cnt = size / sizeof(pkt);
	LOG(" pkt_cnt:%zu size:%zu ", pkt_cnt, size);
	size_t reminder = size % sizeof(pkt);
	if (reminder != 0)
		ERR("bad packet; reminder != 0");
	for (size_t idx = 0; idx < pkt_cnt; ++idx)
	{
		pkt_food_set_t pkt;
		memcpy(&pkt, buf + (sizeof(pkt) * idx), sizeof(pkt));
		food_t food{be16toh(pkt.x), be16toh(pkt.y), pkt.color, pkt.size / 5, false};
		food_list.push_back(food);
	}
}

// f b
void game_t::pkt_food_add(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_food_set_t)){ LOG("wrong size:%zu", size); return; }
	pkt_food_set_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	LOG("%u %u %u %u", pkt.color, be16toh(pkt.x), be16toh(pkt.y), pkt.size / 5);
	food_t food{be16toh(pkt.x), be16toh(pkt.y), pkt.color, pkt.size / 5, false};
	decltype(food_list)::iterator food_it = find_by_xy(food_list, food);
	if (food_it != food_list.end())
		return;
	food_list.push_back(food);
}

// c
void game_t::pkt_food_eat(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_food_eat_t)){ LOG("wrong size:%zu", size); return; }
	pkt_food_eat_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	xy_t xy{be16toh(pkt.x), be16toh(pkt.y)};
	LOG("%s snake_id:%u", to_str(xy).c_str(), be16toh(pkt.snake_id));
	for (food_t& food : food_list)
		if (food == xy)
			food.eaten = true;
}

// g
void game_t::pkt_snake_mov(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_snake_mov_t)){ LOG("wrong size:%zu", size); return; }
	pkt_snake_mov_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	snake_t& snake = snake_get(snake_id);
	if (my_snake_id == snake_id_invalid)
		my_snake_id = snake_id;
	snake.move(xy_t{be16toh(pkt.x), be16toh(pkt.y)});
	LOG("%zu %s", snake_id, to_str(snake.part_list[0]).c_str());
}

void game_t::pkt_snake_mov_G(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_snake_mov_G_t)){ LOG("wrong size:%zu", size); return; }
	pkt_snake_mov_G_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	snake_t& snake = snake_get(snake_id);
	if (snake.snake_length == 0)
		return;
	if (snake.part_list.empty())
		return;

	xy_t head{snake.part_list.front()};
	head.x += pkt.x - 128;
	head.y += pkt.y - 128;
	LOG("%zu %s", snake_id, to_str(head).c_str());
	snake.move(head);
}

// N
void game_t::pkt_snake_mov_inc(const uint8_t* buf, size_t size)
{
	if (size != sizeof(pkt_snake_mov_inc_t)){ LOG("wrong size:%zu", size); return; }
	pkt_snake_mov_inc_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	snake_t& snake = snake_get(snake_id);
	snake.snake_length++;

	xy_t head{snake.part_list.front()};
	head.x += pkt.x - 128;
	head.y += pkt.y - 128;
	LOG("%zu %s length:%zu", snake_id, to_str(head).c_str(), snake.snake_length);
	snake.move(head);
}

// e
void game_t::pkt_snake_rot_e(const uint8_t* buf, size_t size)
{
	size_t snake_id = 0;
	float angle = invalid_angle;
	float wangle = invalid_angle;
	float speed = invalid_angle;
	switch (size)
	{
		case sizeof(pkt_snake_rot_e_3_t):
		{
			pkt_snake_rot_e_3_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			snake_id = be16toh(pkt.snake_id);
			// ang * pi2 / 256 (current snake angle in radians, clockwise from (1, 0))
			angle = pkt.angle * M_PI * 2 / 256;
			snake_get(snake_id).rot_angle = angle;
			break;
		}
		case sizeof(pkt_snake_rot_e_4_t):
		{
			pkt_snake_rot_e_4_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			snake_id = be16toh(pkt.snake_id);
			angle = 1. * pkt.angle * M_PI * 2 / 256;
			speed = 1. * pkt.speed / 18;
			break;
		}
		case sizeof(pkt_snake_rot_e_5_t):
		{
			pkt_snake_rot_e_5_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			snake_id = be16toh(pkt.snake_id);
			angle = pkt.angle * M_PI * 2 / 256;
			wangle = pkt.wangle * M_PI * 2 / 256;
			speed = 1. * pkt.speed / 18;
			break;
		}
		default:
			LOG("wrong size:%zu", size);
			return;
	}
	LOG("size:%zu %zu angle:%f wangle:%f speed:%f", size, snake_id, angle, wangle, speed);
	snake_t& snake = snake_get(snake_id);
	if (angle != invalid_angle)
		snake.rot_angle = angle;
	if (wangle != invalid_angle)
		snake.rot_wangle = wangle;
	if (speed != invalid_angle)
		snake.speed = speed;
}

// 3
void game_t::pkt_snake_rot_3(const uint8_t* buf, size_t size)
{
	if (size != sizeof(pkt_snake_rot_5_t)){ LOG("wrong size:%zu", size); return; }
	pkt_snake_rot_5_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	snake_t& snake = snake_get(snake_id);
	// ang * pi2 / 256 (current snake angle in radians, clockwise from (1, 0))
	float angle = pkt.angle * M_PI * 2 / 256;
	float wangle = pkt.wangle * M_PI * 2 / 256;
	LOG("size:%zu %zu angle:%f wangle:%f", size, snake_id, angle, wangle);
	snake.rot_angle = angle;
	snake.rot_wangle = wangle;
	snake.rot_dir = rot_dir_cw;
}

void game_t::pkt_snake_rot_4(const uint8_t* buf, size_t size)
{
	if (size != sizeof(pkt_snake_rot_4_5_t)){ LOG("wrong size:%zu", size); return; }
	pkt_snake_rot_4_5_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	snake_t& snake = snake_get(snake_id);
	// ang * pi2 / 256 (current snake angle in radians, clockwise from (1, 0))
	float angle = pkt.angle * M_PI * 2 / 256;
	float wangle = pkt.wangle * M_PI * 2 / 256;
	float speed = pkt.speed / 18;
	LOG("size:%zu %zu angle:%f wangle:%f speed:%f", size, snake_id, angle, wangle, speed);
	snake.rot_angle = angle;
	snake.rot_wangle = wangle;
	snake.speed = speed;
}

void game_t::pkt_snake_rot_5(const uint8_t* buf, size_t size)
{
	if (size != sizeof(pkt_snake_rot_5_t)){ LOG("wrong size:%zu", size); return; }
	pkt_snake_rot_5_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	// ang * pi2 / 256 (current snake angle in radians, clockwise from (1, 0))
	float angle = pkt.angle * M_PI * 2 / 256;
	LOG("size:%zu %zu angle:%f pkt.angle:%u", size, snake_id, angle, pkt.angle);
	snake_get(snake_id).rot_angle = angle;
}

// n
void game_t::pkt_snake_inc(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_snake_inc_t)){ LOG("wrong size:%zu", size); return; }
	pkt_snake_inc_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	snake_t& snake = snake_get(snake_id);
	snake.snake_length++;
	snake.fam = 1. * be24toh(pkt.fam) / 16777215;

	snake.move(xy_t{be16toh(pkt.x), be16toh(pkt.y)});

	LOG("%zu %s fam:%f",
		snake_id,
		to_str(snake_get(snake_id).part_list[0]).c_str(),
		snake_get(snake_id).fam);
}

// s
void game_t::pkt_snake(const uint8_t* buf, size_t size)
{
	if (size == sizeof(pkt_snake_t))
	{
		pkt_snake_t pkt;
		memcpy(&pkt, buf, sizeof(pkt));
		size_t snake_id = be16toh(pkt.snake_id);
		LOG("%zu reason:%u size:%zu", snake_id, pkt.reason, size);
		if (pkt.reason == 1)
		{
			snake_get(snake_id).dead = true;
			if (snake_id == my_snake_id)
			{
				LOG("my snake dead");
			}
		}
		if (snake_id != my_snake_id)
		{
			snake_id_list.erase(std::remove(snake_id_list.begin(), snake_id_list.end(), snake_id));
			snake_get(snake_id).clear();
		}
		return;
	}

	if (size < sizeof(pkt_snake_data_t)){ LOG("wrong size:%zu", size); return; }
	pkt_snake_data_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	snake_t& snake = snake_get(snake_id);
	snake.clear();
	xy_t head{be24toh(pkt.x) / 5, be24toh(pkt.y) / 5};
	const uint8_t* skin_len_p = reinterpret_cast<const uint8_t*>
		(buf + sizeof(pkt_snake_data_t) + pkt.name_len);
	const pkt_snake_data_tail_t* tail_p = reinterpret_cast<const pkt_snake_data_tail_t*>
		(skin_len_p + 1 + (*skin_len_p));
	const pkt_snake_data_part_t* part_p = reinterpret_cast<const pkt_snake_data_part_t*>
		(tail_p + 1);
	const size_t part_cnt =
		(size - (reinterpret_cast<const uint8_t*>(part_p) - buf)) / sizeof(pkt_snake_data_part_t);
	char name[80];
	size_t name_len = std::min(sizeof(name) - 1, static_cast<size_t>(pkt.name_len));
	memcpy(name, buf + sizeof(pkt), name_len);
	name[name_len] = 0;
	pkt_snake_data_tail_t pkt_tail;
	memcpy(&pkt_tail, reinterpret_cast<const uint8_t*>(tail_p), sizeof(pkt_tail));
	xy_t tail{be24toh(pkt_tail.x) / 5, be24toh(pkt_tail.y) / 5};
	LOG("%zu size:%zu head:%d:%d tail:%d:%d part_cnt:%zu skin:%d name:%s",
		snake_id, size, head.x, head.y, tail.x, tail.y, part_cnt, pkt.skin, name);

	snake.fam = 1. * be24toh(pkt.fam) / 16777215;
	snake.rot_angle = 1. * be24toh(pkt.angle) * M_PI * 2 / 16777215;
	snake.rot_wangle = 1. * be24toh(pkt.wangle) * M_PI * 2 / 16777215;
	snake.speed = 1. * be16toh(pkt.speed) / 1000;
	strncpy(snake.name, name, sizeof(snake.name) - 1);

	if (snake_id_list.end() == std::find(snake_id_list.begin(), snake_id_list.end(), snake_id))
		snake_id_list.push_back(snake_id);

	snake.skin = pkt.skin;
	snake.tstamp_data = uptime_us();
	if (snake.head == xy_t{0, 0} && !snake.part_list.empty())
		snake.head = snake.part_list[0];

	std::deque<xy_t> new_part_list;
	new_part_list.push_front(tail);
	for (size_t idx = 0; idx < part_cnt; ++idx)
	{
		pkt_snake_data_part_t pkt_part;
		memcpy(&pkt_part, reinterpret_cast<const uint8_t*>(&part_p[idx]), sizeof(pkt_part));
		xy_t part{
			(pkt_part.x - 127) / 2,
			(pkt_part.y - 127) / 2
			};
		part = {
			new_part_list.front().x + part.x,
			new_part_list.front().y + part.y
		};
		new_part_list.push_front(part);
	}
	new_part_list.push_back(head);
	for (xy_t part : new_part_list)
		snake.part_list.push_back(part);
	snake.snake_length = snake.part_list.size();
	snake.head = snake.part_list[0];

	if (my_snake_id == snake_id_invalid)
	{
		my_snake_id = snake_id;
		draw_ctx_update();
	}
}

static std::deque<prey_t>::iterator prey_list_find_id(std::deque<prey_t>& prey_list, size_t id)
{
	return std::find_if(
			prey_list.begin(), prey_list.end(),
			[id](const prey_t& prey)
			{ return prey.id == id; }
			);
}

// y
void game_t::pkt_prey(const uint8_t* buf, size_t size)
{
	pkt_prey_rem_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	prey_t prey{};
	prey.id = be16toh(pkt.prey_id);
	std::deque<prey_t>::iterator prey_it = prey_list_find_id(prey_list, prey.id);
	switch(size)
	{
		case sizeof(pkt_prey_eat_t):
		{
			pkt_prey_eat_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			prey.id = be16toh(pkt.prey_id);
			if (prey_it == prey_list.end())
			{
				LOG("prey.id:%zu not found", prey.id);
				return;
			}
			prey_it->eaten = true;
			break;
		}
		case sizeof(pkt_prey_rem_t):
		{
			pkt_prey_rem_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			prey.id = be16toh(pkt.prey_id);
			if (prey_it == prey_list.end())
			{
				LOG("prey.id:%zu not found", prey.id);
				return;
			}
			prey_it->eaten = true;  // not realy eaten just mark to remove
			break;
		}
		case sizeof(pkt_prey_add_t):
		{
			pkt_prey_add_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			if (prey_it != prey_list.end())
			{
				LOG("prey.id:%zu found", prey.id);
				return;
			}
			prey.xy = xy_t{be24toh(pkt.x) / 5, be24toh(pkt.y) / 5};
			prey.color = pkt.color;
			prey.size = pkt.size / 5;
			prey.rot_angle = 1. * be24toh(pkt.angle) * M_PI * 2 / 16777215;  // value * 2 * PI / 16777215
			prey.rot_wangle = 1. * be24toh(pkt.wangle) * M_PI * 2 / 16777215;
			prey.speed = be16toh(pkt.wangle) / 1000;
			prey.dir = static_cast<rot_dir_t>(pkt.dir - 48);
			prey.tstamp_data = uptime_us();
			prey_list.emplace_back(prey);
			LOG(" prey.id:%zu %s size:%d color:%d rot_angle:%f rot_wangle:%f speed:%f dir:%d",
				prey.id, to_str(prey.xy).c_str(), prey.size, prey.color,
				prey.rot_angle, prey.rot_wangle, prey.speed, prey.dir);
			break;
		}

		default:
			LOG("wrong size:%zu", size);
			return;
	}
}

// j
void game_t::pkt_prey_upd(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_prey_upd_t)){ LOG("wrong size:%zu", size); return; }
	pkt_prey_upd_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t prey_id = be16toh(pkt.prey_id);
	std::deque<prey_t>::iterator prey_it = prey_list_find_id(prey_list, prey_id);
	if (prey_it == prey_list.end())
	{
		LOG("prey.id:%zu not found", prey_id);
		return;
	}
	prey_t& prey = *prey_it;
	prey.xy_prev = prey.xy;
	prey.xy = xy_t{be16toh(pkt.x) * 3 + 1, be16toh(pkt.y) * 3 + 1};
	uint64_t now_us = uptime_us();
	prey.tstamp_data = now_us;
	ssize_t size_ext = size - sizeof(pkt_prey_upd_t);
	buf += sizeof(pkt_prey_upd_t);
	if (size_ext <= 0)
		return;
	float angle = invalid_angle;
	float wangle = invalid_angle;
	float speed = invalid_angle;
	static const size_t invalid_dir = 4;
	size_t dir = invalid_dir; // (0: not turning; 1: counter-clockwise; 2: clockwise)
	switch(size_ext)
	{
		case sizeof(pkt_prey_upd_ext_2_t):
		{
			pkt_prey_upd_ext_2_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			speed = be16toh(pkt.speed);
			break;
		}
		case sizeof(pkt_prey_upd_ext_3_t):
		{
			pkt_prey_upd_ext_3_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			angle = be24toh(pkt.angle);
			break;
		}
		case sizeof(pkt_prey_upd_ext_4_t):
		{
			pkt_prey_upd_ext_4_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			wangle = be24toh(pkt.wangle);
			break;
		}
		case sizeof(pkt_prey_upd_ext_5_t):
		{
			pkt_prey_upd_ext_5_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			wangle = be24toh(pkt.wangle);
			speed = be16toh(pkt.speed);
			break;
		}
		case sizeof(pkt_prey_upd_ext_6_t):
		{
			pkt_prey_upd_ext_6_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			wangle = be24toh(pkt.wangle);
			speed = be16toh(pkt.speed);
			break;
		}
		case sizeof(pkt_prey_upd_ext_7_t):
		{
			pkt_prey_upd_ext_7_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			dir = pkt.dir;
			angle = be24toh(pkt.angle);
			wangle = be24toh(pkt.wangle);
			break;
		}
		case sizeof(pkt_prey_upd_ext_9_t):
		{
			pkt_prey_upd_ext_9_t pkt;
			memcpy(&pkt, buf, sizeof(pkt));
			dir = pkt.dir;
			angle = be24toh(pkt.angle);
			wangle = be24toh(pkt.wangle);
			speed = be16toh(pkt.speed);
			break;
		}
		default:
			LOG("wrong size_ext:%zu", size_ext);
			return;
	}
	if (angle != invalid_angle)
		prey.rot_angle = angle * M_PI * 2 / 16777215;
	if (wangle != invalid_angle)
		prey.rot_wangle = wangle * M_PI * 2 / 16777215;
	if (speed != invalid_angle)
		prey.speed = speed / 1000;
	if (dir != invalid_dir)
		prey.dir = static_cast<rot_dir_t>(dir - 48);

	LOG("id:%zu size_ext:%zu rot_angle:%f rot_wangle:%f speed:%f dir:%d dist:%d dt:%zu",
		prey_id, size_ext, prey.rot_angle, prey.rot_wangle, prey.speed, prey.dir,
		distance(prey.xy, prey.xy_prev), now_us - prey.tstamp_data);
}

// v
void game_t::pkt_end(const uint8_t* buf, size_t size)
{
	if (size < 1){ LOG("wrong size:%zu", size); return; }
	snake_get(my_snake_id).dead = true;
	(void)buf;
	LOG("my snake dead reason:%u", buf[0]);
}

// h
void game_t::pkt_snake_fam(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_snake_fam_t)){ LOG("wrong size:%zu", size); return; }
	// 	Update the fam-value (used for length-calculation) of a snake.
	// 	fam is a float value (usually in [0 .. 1.0]) representing a
	// 	body part ratio before changing snake length sct in body parts.
	// 	Snake gets new body part when fam reaches 1, and looses 1, when fam reaches 0.
	pkt_snake_fam_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	snake_t& snake = snake_get(snake_id);
	snake.fam = 1. * be24toh(pkt.fam) / 16777215;
	LOG("%zu fam:%f length:%zu", snake_id, snake.fam, snake.snake_length);
}

// r
void game_t::pkt_snake_rem_part(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_snake_fam_t)){ LOG("wrong size:%zu", size); return; }
	pkt_snake_fam_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	size_t snake_id = be16toh(pkt.snake_id);
	snake_t& snake = snake_get(snake_id);
	snake.fam = 1. * be24toh(pkt.fam) / 16777215;  // TODO
	LOG("%zu fam:%f length:%zu", snake_id, snake.fam, snake.snake_length);
	if (snake.snake_length == 0)
		return;
	snake.snake_length--;
	snake.part_list_trim();
}

// u
void game_t::pkt_minimap(const uint8_t* buf, size_t size)
{
	LOG(" size:%zu", size);
	const uint8_t* data = buf;
	minimap = {};
	size_t mapPos = 0;
	for (size_t dataPos = 0; dataPos < size; dataPos++)
	{
		int32_t value = data[dataPos];
		if (value >= 128)
		{
			value -= 128;
			mapPos += value;
			if (mapPos > minimap.size())
				break;
		} else
		{
			for (int i = 0; i < 7; i++)
			{
				if ((value & (1 << (6 - i))) != 0)
					minimap[mapPos] = 1;
				mapPos++;
				if (mapPos > minimap.size())
					break;
			}
		}
	}
}

// l
void game_t::pkt_leaderboard(const uint8_t* buf, size_t size)
{
	if (size < sizeof(pkt_leaderboard_t)){ LOG("wrong size:%zu", size); return; }
	pkt_leaderboard_t pkt;
	memcpy(&pkt, buf, sizeof(pkt));
	leaderboard.rank = be16toh(pkt.rank);
	leaderboard.player_count = be16toh(pkt.player_count);
	LOG(" rank:%zu player_count:%zu", leaderboard.rank, leaderboard.player_count);
	const char* cursor = reinterpret_cast<const char*>(buf) + sizeof(pkt_leaderboard_t);
	const char* end = reinterpret_cast<const char*>(buf) + size;
	size_t idx = 0;
	while (cursor < end && idx < leaderboard.player_list.size())
	{
		pkt_leaderboard_player_t pkt_player;
		memcpy(&pkt_player, cursor, sizeof(pkt_player));
		cursor += sizeof(pkt_leaderboard_player_t);
		leaderboard_player_t& player = leaderboard.player_list[idx];
		player.name = {};
		size_t name_len = std::min(static_cast<size_t>(pkt_player.name_len), player.name.size() - 1);
		strncpy(player.name.data(), cursor, name_len);
		player.body_part_count = be16toh(pkt_player.body_part_count);
		player.font_color = pkt_player.font_color;
		player.fam = 1. * be24toh(pkt_player.fam) / 16777215;

		cursor += pkt_player.name_len;
		++idx;
	}
	leaderboard.have_data = true;
}

void game_t::pkt_pong(const uint8_t* buf, size_t size)
{
	(void)buf;
	(void)size;
	if (ping_ctx.wait_pong == false)
		return;
	ping_ctx.wait_pong = false;
	ping_ctx.ping_pong_us = uptime_us() - ping_ctx.ping_tstamp;
	ping_ctx.ping_pong_avg_us += ping_ctx.ping_pong_us;
	ping_ctx.ping_pong_avg_us /= 2;
}

bool game_t::draw_ctx_update()
{
	if (my_snake_id == snake_id_invalid)
		return false;
	xy_t game_view_center_prev = draw_ctx.game_view_center;
	draw_ctx.game_view_center = snake_get(my_snake_id).head;
	draw_ctx.scale = 0.5;

	const coordinate_t max_coord = config.game_radius * 2;
	xy_t ul = {max_coord, max_coord};
	xy_t lr = {-1, -1};
	for (xy_t sect : sector_list)
	{
		sect.x *= config.sector_size;
		sect.y *= config.sector_size;
		if (ul.x > sect.x)
			ul.x = sect.x;
		if (ul.y > sect.y)
			ul.y = sect.y;
		if (lr.x < sect.x + config.sector_size)
			lr.x = sect.x + config.sector_size;
		if (lr.y < sect.y + config.sector_size)
			lr.y = sect.y + config.sector_size;
	}
	draw_ctx.sect_rect.ul = ul;
	draw_ctx.sect_rect.lr = lr;
	draw_ctx.width = lr.x - ul.x;
	draw_ctx.height = lr.y - ul.y;
	draw_ctx.game_view_rect = {ul, lr};

	return game_view_center_prev != draw_ctx.game_view_center;
}

template <typename Txy>
xy_t game_t::screen_xy(const Txy& game_xy)
{
	xy_t xy{
		(game_xy.x - draw_ctx.game_view_center.x) * draw_ctx.scale,
		(game_xy.y - draw_ctx.game_view_center.y) * draw_ctx.scale
		};

	xy.x += screen.width / 2;
	xy.y += screen.height / 2;
	return xy;
}

template <typename Txy>
xy_t game_t::game_xy(const Txy& screen_xy)
{
	xy_t xy{
		(screen_xy.x - screen.width / 2) / draw_ctx.scale + draw_ctx.game_view_center.x,
		(screen_xy.y - screen.height / 2) / draw_ctx.scale + draw_ctx.game_view_center.y
		};

	return xy;
}

typedef screen_sdl_t::color_t color_t;
static const color_t white(255, 255, 255);
static const color_t red(255, 0, 0);
static const color_t green(0, 255, 0);
static const color_t blue(0, 0, 255);

void game_t::draw()
{
	static uint64_t draw_tstamp = 0;
	uint64_t now_us = uptime_us();
	size_t delta_us = now_us - draw_tstamp;
	draw_tstamp = now_us;

	if (draw_ctx_update() || my_snake_dead())
		screen.clear();
	draw_background();
	draw_leaderboard();
	int32_t network_delay = ping_ctx.ping_pong_avg_us / 2;
	draw_snake_list(now_us + network_delay);
	draw_prey(now_us + network_delay);
	draw_food();
	draw_minimap();

	char buf[32];
	snprintf(buf, sizeof(buf), "%6lu", run_time_us() / 1000);
	screen.text(20, 20, white, 15, buf);
	fps = (fps + (1000000. / delta_us)) / 2;
	snprintf(buf, sizeof(buf), "FPS:%6.2f", fps);
	screen.text(20, 40, white, 15, buf);

	screen.present();
	have_data = false;
}

size_t snake_body_part_radius(size_t parts_count, float scale)
{
	float sc = fminf(6.0f, 1.0f + 1.f * (parts_count - 2) / 106.0f);
	float sbpr = 29.0f * 0.5f /* render mode 2 const */ * sc;
	sbpr *= scale;
	return static_cast<size_t>(sbpr);
}

uint8_t rr_list[] = {192, 144, 128, 128, 238, 255, 255, 255, 224, 255, 144, 80, 255, 40, 100, 120, 72, 160, 255, 56, 56, 78, 255, 101, 128, 60, 0, 217, 255, 144, 32, 240, 240, 240, 240, 32, 40, 104, 0, 104, /*0*/ 128};
uint8_t gg_list[] = {128, 153, 208, 255, 238, 160, 144, 64, 48, 255, 153, 80, 192, 136, 117, 134, 84, 80, 224, 68, 68, 35, 86, 200, 132, 192, 255, 69, 64, 144, 32, 32, 240, 144, 32, 240, 60, 128, 0, 40, /*0*/ 208};
uint8_t bb_list[] = {255, 255, 208, 128, 112, 96, 144, 64, 224, 255, 255, 80, 80, 96, 255, 255, 255, 255, 64, 255, 255, 192, 9, 232, 144, 72, 83, 69, 64, 144, 240, 32, 32, 32, 240, 32, 173, 255, 112, 170, /*0*/ 208};

static const size_t max_skin_cv = sizeof(rr_list) / sizeof(*rr_list);
// TODO make skin->color map like in game832434.js:setSkin()

void game_t::draw_part(xy_t xy, size_t radius, size_t snake_id)
{
	size_t color_idx = snake_get(snake_id).skin;
	if (color_idx >= max_skin_cv)
		color_idx = color_idx % max_skin_cv;
	color_t color{rr_list[color_idx], gg_list[color_idx], bb_list[color_idx]};

	screen.octagon(xy.x, xy.y, radius, color);
}

void game_t::draw_snake(size_t snake_id, uint64_t now_us)
{
	draw_snake_prepare1(snake_id, now_us);
	snake_t& snake = snake_get(snake_id);

	const rect_t screen_rect{xy_t{0, 0}, xy_t{screen.width, screen.height}};
	assert(draw_ctx.scale > 0.);
	size_t radius = snake_body_part_radius(snake.snake_length, draw_ctx.scale);
	snake.tstamp_draw = now_us;
	size_t length = std::min(snake.snake_length, snake.part_list.size());
	draw_part(screen_xy(snake.head), radius, snake_id);
	for (size_t idx = 0; idx < length; ++idx)
	{
		xy_t xy = screen_xy(snake.part_list[idx]);
		if (!screen_rect.has(xy))
			continue;
		draw_part(xy, radius, snake_id);
	}
	if (snake.part_list.size() < 2)
	{
		return;
	}

	xy_t xy = screen_xy(snake_get(snake_id).head);
	if (strlen(snake.name) > 0)
		screen.text(xy.x, xy.y, white, 15, snake.name);
}

float snake_heading(const snake_t& snake)
{
 	if (snake.part_list.size() < 2)
		return invalid_angle;
	return xy_angle(snake.part_list[1], snake.part_list[0]);
}

rot_dir_t rot_dir_calc(float from, float to)
{
	// angle [0 : 2 * M_PI)
	// angle increase clockwise
	if (from > M_PI * 3 && to <= M_PI * 3)
		return rot_dir_cw;
	if (from > to)
		return rot_dir_ccw;
	return rot_dir_cw;
}

void game_t::draw_snake_prepare1(size_t snake_id, uint64_t now_us)
{
	static const float angle_tolerance = (2 * M_PI) / (250 + 1);  // TODO 250 - part of the protocol
	snake_t& snake = snake_get(snake_id);
	if (snake.dead)
		return;
	if (snake.part_list.empty())
		return;
	float heading = snake_heading(snake);
	float rot_angle = norm_angle(snake.rot_angle);
	if (::abs(snake.rot_angle - invalid_angle) < angle_tolerance)
		rot_angle = heading;
	size_t delta_us = now_us - snake.tstamp_data;
	coordinate_t speed_step_ps = snake.speed;
	static const float speed_ko = 0.6;
	float speed = speed_step_ps * snake_step_distance * speed_ko;
	if (::abs(snake.speed - invalid_angle) < angle_tolerance)
		speed = snake_speed * snake_step_distance;

	coordinate_t delta_distance = speed * delta_us / 1000000;
	if (delta_distance == 0)
	{
		snake.head = snake.part_list[0];
		return;
	}
	if (snake_id == my_snake_id)
	{
		LOG("delta_us:%zu delta_distance:%d snake.speed:%f speed:%f",
			delta_us, delta_distance, snake.speed, speed);
	}
	xy_t xy_ext{snake.part_list[0]};
	xy_ext.x += ::cosf(rot_angle) * delta_distance;
	xy_ext.y += ::sinf(rot_angle) * delta_distance;

	snake.head = xy_ext;

#ifdef DEBUG_LOG
	if (delta_us < draw_period_us)
	{
		if (snake_id == my_snake_id)
		{
			float dist_error = 0.;
			coordinate_t dist_p0 = distance(snake.prev, snake.part_list[0]);
			coordinate_t dist_ph = distance(snake.prev, snake.head);
			dist_error = dist_p0 - dist_ph;
			LOG("delta_us:%zu dist_error:%f dist_p0:%d dist_ph:%d",
				delta_us, dist_error, dist_p0, dist_ph);
		}
	}
#endif  // DEBUG_LOG
}

void game_t::draw_snake_list(uint64_t now_us)
{
	if (!my_snake_dead())
		draw_snake(my_snake_id, now_us);

	for (size_t snake_id : snake_id_list)
	{
		if (snake_id == my_snake_id)
			continue;
		draw_snake(snake_id, now_us);
	}
}

void game_t::draw_food()
{
	const rect_t screen_rect{xy_t{0, 0}, xy_t{screen.width, screen.height}};
	for (food_t xy : food_list)
	{
		xy_t xy_scr = screen_xy(xy);
		if (xy.eaten)
			continue;
		if (!screen_rect.has(xy_scr))
			continue;
		size_t color_idx = xy.color;
		if (color_idx >= max_skin_cv)
			color_idx = color_idx % max_skin_cv;
		color_t color{rr_list[color_idx], gg_list[color_idx], bb_list[color_idx]};
		screen.octastar(xy_scr.x, xy_scr.y, xy.size * draw_ctx.scale, color);
	}

	food_list.erase(
		std::remove_if(food_list.begin(), food_list.end(),
		[](const food_t& food){ return food.eaten == true; }), food_list.end());

	food_list.erase(
		std::remove_if(food_list.begin(), food_list.end(),
		[this](const food_t& food){ return !draw_ctx.game_view_rect.has(food); }), food_list.end());
}

void game_t::draw_prey(uint64_t now_us)
{
	const rect_t screen_rect{xy_t{0, 0}, xy_t{screen.width, screen.height}};
	for (prey_t prey : prey_list)
	{
		size_t delta_us = now_us - prey.tstamp_data;
		float speed = 1000. * prey.speed / 8. / 4.;
		if (speed > 200.)
			speed = 200.;
		coordinate_t dist = speed * delta_us / 1000000.;

		xy_t xy_ext{prey.xy};
		// manu2 angle in rad per 8ms at which prey can turn) 0.028 0.028
		float delta_angle = config.manu2  * delta_us / 1000. / 8. * 0.4;
		float angle = norm_angle(prey.rot_angle);
		switch(prey.dir)
		{
			case rot_dir_no: angle = norm_angle(prey.rot_wangle); break;
			case rot_dir_ccw: angle -= delta_angle; break;
			case rot_dir_cw: angle += delta_angle; break;
			default: break;
		}
		angle = norm_angle(angle);
		xy_ext.x += ::cosf(angle) * dist;
		xy_ext.y += ::sinf(angle) * dist;

		xy_t xy_scr = screen_xy(xy_ext);
		if (prey.eaten)
			continue;
		if (!screen_rect.has(xy_scr))
			continue;
		size_t color_idx = prey.color;
		if (color_idx >= max_skin_cv)
			color_idx = color_idx % max_skin_cv;
		color_t color{rr_list[color_idx], gg_list[color_idx], bb_list[color_idx]};
		screen.octagon(xy_scr.x, xy_scr.y, prey.size * draw_ctx.scale + 3, color);
		screen.octastar(xy_scr.x, xy_scr.y, prey.size * draw_ctx.scale + 2, color);

	}

	prey_list.erase(
		std::remove_if(prey_list.begin(), prey_list.end(),
		[](const prey_t& prey){ return prey.eaten == true; }), prey_list.end());
}

void game_t::draw_minimap()
{
	const float scale = 4.;
	const xy_t map_pos{screen.width - (80 * scale), screen.height - (80 * scale)};
	rect_t map_rect{map_pos, xy_t{map_pos.x + (80 * scale), map_pos.y + (80 * scale)}};
	xy_t map_ctr = map_rect.center();

	for (size_t idx = 0; idx < minimap.size(); idx++)
	{
		if (!minimap[idx])
			continue;
		xy_t xy{idx % 80, idx / 80};
		xy.x = (xy.x * scale) + map_pos.x;
		xy.y = (xy.y * scale) + map_pos.y;
		screen.point(
			xy.x,
			xy.y,
			white
		);
	}
	screen.circle(map_ctr.x, map_ctr.y, map_rect.width() / 2, white);

	const float game_scale = 80. / (config.game_radius * 2);
	xy_t xy = draw_ctx.game_view_center;
	xy.x = xy.x * game_scale * scale + map_pos.x;
	xy.y = xy.y * game_scale * scale + map_pos.y;
	screen.octagon(xy.x, xy.y, 4, red);
}

void game_t::draw_leaderboard()
{
	if (!leaderboard.have_data)
		return;
	char buf[80];
	for (size_t idx = 0; idx < leaderboard.player_list.size(); ++idx)
	{
		leaderboard_player_t& player = leaderboard.player_list[idx];
		snprintf(buf, sizeof(buf), "%zu: %-15s",
			idx + 1, player.name.data());
		buf[15] = 0;
		size_t color_idx = player.font_color;
		if (color_idx >= max_skin_cv)
			color_idx = color_idx % max_skin_cv;
		color_t color{rr_list[color_idx], gg_list[color_idx], bb_list[color_idx]};
		screen.text(screen.width - 200, 20 + (idx * 20), color, 15, buf);
		snprintf(buf, sizeof(buf), "%6d",
			score.get(player.body_part_count, player.fam));
		screen.text(screen.width - 50, 20 + (idx * 20), color, 15, buf);
	}

	snprintf(buf, sizeof(buf), "Your length: %d",
		score.get(snake_get(my_snake_id).snake_length, snake_get(my_snake_id).fam));
	screen.text(20, screen.height - 40, white, 15, buf);
	snprintf(buf, sizeof(buf), "Your rank %zu of %zu", leaderboard.rank, leaderboard.player_count);
	screen.text(20, screen.height - 20, white, 15, buf);
}

void game_t::draw_background()
{
	xy_t& ul = draw_ctx.game_view_rect.ul;
	xy_t& ctr = draw_ctx.game_view_center;
	xy_t game_ctr{config.game_radius, config.game_radius};
	const coordinate_t view_radius = distance(ctr, ul);
	if (distance(ctr, game_ctr) < config.game_radius - view_radius)
		return;
	for (size_t idx = 0; idx < edge_points.size() - 1; ++idx)
	{
		xy_t xy1 = screen_xy(edge_points[idx]);
		xy_t xy2 = screen_xy(edge_points[idx + 1]);
		screen.line(xy1.x, xy1.y, xy2.x, xy2.y, red);
	}
	xy_t xy1 = screen_xy(edge_points[0]);
	xy_t xy2 = screen_xy(edge_points[edge_points.size() - 1]);
	screen.line(xy1.x, xy1.y, xy2.x, xy2.y, red);

	return;
}

void game_t::edge_points_calc()
{
	// TODO I am killed when not over config.game_radius. Sectors? Not enogh.
	coordinate_t game_radius = config.game_radius - config.sector_size;
	xy_t game_ctr{config.game_radius, config.game_radius};
	float ang_stp_div = edge_points.size();  // 360
	float ang_stp  = M_PI * 2 / ang_stp_div;
	for (int32_t cnt = 0; cnt < M_PI * 2 / ang_stp; ++cnt)
	{
		edge_points[cnt] = game_ctr;
		edge_points[cnt].x += ::cosf(norm_angle(ang_stp * cnt)) * game_radius;
		edge_points[cnt].y += ::sinf(norm_angle(ang_stp * cnt)) * game_radius;
	}
}

float game_t::mouse_angle()
{
		xy_t mos;
		screen.mouse_position(mos.x, mos.y);
		xy_t mos_game_xy = game_xy(mos);
		xy_t head = snake_get(my_snake_id).head;
		float angle = xy_angle(head, mos_game_xy) + (M_PI * 2);
		angle = norm_angle(angle);
		return angle;
}

bool game_t::my_snake_dead()
{
	return snake_get(my_snake_id).dead;
}

