#ifndef PACKET_TO_CLIENT_H
#define PACKET_TO_CLIENT_H

#include <stdint.h>
#include <endian.h>

static inline uint32_t be24toh(uint32_t int24)
{
#ifdef __LITTLE_ENDIAN
	int24 = int24 << 8;
#elif __BIG_ENDIAN
#else
#error endiannes is not defined
#endif
	return be32toh(int24);
}

typedef uint8_t packet_type_server_t;
// TODO think attribute((packed))
#pragma pack(push, 1)
struct pkt_hdr_t
{
	uint16_t client_time = 0;  // 2 bytes - time since last message from client
	packet_type_server_t packet_type;  // 1 byte - packet type
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_init_t
{
//~ 	3-5 	int24 	Game Radius 	16384 	21600
	uint32_t game_radius:24;
//~ 	6-7 	int16 	mscps (maximum snake length in body parts units) 	300 	411
	uint16_t mscps;
//~ 	8-9 	int16 	sector_size 	480 	300
	uint16_t sector_size;
//~ 	10-11 	int16 	sector_count_along_edge (unused in the game-code) 	130 	144
	uint16_t sector_count_along_edge;
//~ 	12 	int8 	spangdv (value / 10) (coef. to calculate angular speed change depending snake speed) 	4.8 	4.8
	uint8_t spangdv;
//~ 	13-14 	int16 	nsp1 (value / 100) (Maybe nsp stands for "node speed"?) 	4.25 	5.39
	uint16_t nsp1;
//~ 	15-16 	int16 	nsp2 (value / 100) 	0.5 	0.4
	uint16_t nsp2;
//~ 	17-18 	int16 	nsp3 (value / 100) 	12 	14
	uint16_t nsp3;
//~ 	19-20 	int16 	mamu (value / 1E3) (basic snake angular speed) 	0.033 	0.033
	uint16_t mamu;
//~ 	21-22 	int16 	manu2 (value / 1E3) (angle in rad per 8ms at which prey can turn) 	0.028 	0.028
	uint16_t manu2;
//~ 	23-24 	int16 	cst (value / 1E3) (snake tail speed ratio ) 	0.43 	0.43
	uint16_t cst;
//~ 	25 	int8 	protocol_version 	2 	11
	uint8_t protocol_version;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_sector_add_t
{
	uint8_t x;  //~ 3 	int8 	x-coordinate of the new sector
	uint8_t y;  //~ 4 	int8 	y-coordinate of the new sector
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_sector_rem_t
{
	uint8_t x;  //~ 3 	int8 	x-coordinate of the new sector
	uint8_t y;  //~ 4 	int8 	y-coordinate of the new sector
};
#pragma pack(pop)


#pragma pack(push, 1)
struct pkt_food_set_t
{
//~ 	3 	int8 	Color?
	uint8_t color;
//~ 	4-5 	int16 	Food X
	uint16_t x;
//~ 	6-7 	int16 	Food Y
	uint16_t y;
//~ 	8 	int8 	value / 5 -> Size
	uint8_t size;
//~ 	One packet can contain more than one food-entity, bytes 3-8 (=6 bytes!) repeat for every entity.

};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_food_eat_t  // c
{
	uint16_t x;
	uint16_t y;
	uint16_t snake_id;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_mov_t // g
{
	uint16_t snake_id;
	uint16_t x;
	uint16_t y;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_mov_inc_t // N
{
	uint16_t snake_id;
	uint8_t x;  // value - 128 + head.x -> x
	uint8_t y;  // value - 128 + head.y -> y
	uint32_t fam : 24;
};
#pragma pack(pop)


#pragma pack(push, 1)
struct pkt_snake_mov_G_t // G
{
	uint16_t snake_id;
	uint8_t x;  // value - 128 + head.x -> x
	uint8_t y;  // value - 128 + head.y -> y
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_rot_e_5_t // e
{
	uint16_t snake_id;
	uint8_t angle;  // ang * pi2 / 256 (current snake angle in radians, clockwise from (1, 0))
	uint8_t wangle;  // wang * pi2 / 256 (target rotation angle snake is heading to)
	uint8_t speed;  // sp / 18 (snake speed?)
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_rot_e_4_t // e
{
	uint16_t snake_id;
	uint8_t angle;  // ang * pi2 / 256 (current snake angle in radians, clockwise from (1, 0))
	uint8_t speed;  // sp / 18 (snake speed?)
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_rot_e_3_t // e
{
	uint16_t snake_id;
	uint8_t angle;  // ang * pi2 / 256 (current snake angle in radians, clockwise from (1, 0))
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_rot_4_5_t // 4
{
	uint16_t snake_id;
	uint8_t angle;
	uint8_t wangle;
	uint8_t speed;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_rot_5_t // 5
{
	uint16_t snake_id;
	uint8_t angle;
	uint8_t wangle;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_t // s
{
//~ 	3-4 	int16 	Snake id
	uint16_t snake_id;
//~ 	5 	int8 	0 (snake left range) or 1 (snake died)
	uint8_t reason;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_data_t // s
{
	uint16_t snake_id;
	uint32_t angle : 24;  // ang * pi2 / 256 (current snake angle in radians, clockwise from (1, 0))
	uint8_t un1;
	uint32_t wangle : 24;  // wang * pi2 / 256 (target rotation angle snake is heading to)
	uint16_t speed;  // sp / 18 (snake speed?)
	uint32_t fam : 24;
	uint8_t skin;
	uint32_t x : 24;
	uint32_t y : 24;
	uint8_t name_len;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_data_tail_t // s
{
	uint32_t x : 24;  // / 5
	uint32_t y : 24;  // / 5
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_data_part_t // s
{
	uint8_t x;  // Next position in relative coords from prev. element (x - 127) / 2
	uint8_t y;  // Next position in relative coords from prev. element (y - 127) / 2
};
#pragma pack(pop)


#pragma pack(push, 1)
struct pkt_snake_fam_t // h
{
	uint16_t snake_id;
	uint32_t fam : 24;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_inc_t // n
{
//~ Move snake  and increase snake body length by 1 body-part
	uint16_t snake_id;
	uint16_t x;
	uint16_t y;
	uint32_t fam : 24;  // value / 16777215 -> fam
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_snake_inc_rel_t // N
{
//~ Move snake  and increase snake body length by 1 body-part
	uint16_t snake_id;
	uint8_t x;  // value - 128 + head.x -> x
	uint8_t y;  // value - 128 + head.y -> y
	uint32_t fam : 24;  // value / 16777215 -> fam
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_leaderboard_t // l
{
	uint8_t rank_in_lb;
	uint16_t rank;
	uint16_t player_count;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_leaderboard_player_t // part of 'l' message
{
	uint16_t body_part_count;  // sct
	uint32_t fam : 24;  // value / 16777215 -> fam
	uint8_t font_color;  // 0 - 8
	uint8_t name_len;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_add_t // y
{
	uint16_t prey_id;
	uint8_t color;
	uint32_t x : 24;  // value / 5 -> x
	uint32_t y : 24;  // value / 5 -> y
	uint8_t size;  // value / 5
	uint8_t dir;  // value - 48 -> direction (0: not turning; 1: turning counter-clockwise; 2: turning clockwise)
	uint32_t wangle : 24;  // value * 2 * PI / 16777215
	uint32_t angle : 24;  // value * 2 * PI / 16777215
	uint16_t speed;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_eat_t // y
{
	uint16_t prey_id;
	uint16_t snake_id;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_rem_t // y
{
	uint16_t prey_id;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_upd_t // j
{
	uint16_t prey_id;
	uint16_t x;  // value * 3 + 1 -> x
	uint16_t y;  // value * 3 + 1 -> y
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_upd_ext_2_t // j
{
	uint16_t speed;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_upd_ext_3_t // j
{
	uint32_t angle : 24;  // value * 2 * PI / 16777215
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_upd_ext_4_t // j
{
	uint8_t dir;
	uint32_t wangle : 24;  // value * 2 * PI / 16777215
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_upd_ext_5_t // j
{
	uint32_t wangle : 24;  // value * 2 * PI / 16777215
	uint16_t speed;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_upd_ext_6_t // j
{
	uint8_t dir;
	uint32_t wangle : 24;  // value * 2 * PI / 16777215
	uint16_t speed;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_upd_ext_7_t // j
{
	uint8_t dir;
	uint32_t angle : 24;  // value * 2 * PI / 16777215
	uint32_t wangle : 24;  // value * 2 * PI / 16777215
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_prey_upd_ext_9_t // j
{
	uint8_t dir;
	uint32_t angle : 24;  // value * 2 * PI / 16777215
	uint32_t wangle : 24;  // value * 2 * PI / 16777215
	uint16_t speed;
};
#pragma pack(pop)

#endif  // PACKET_TO_CLIENT_H
