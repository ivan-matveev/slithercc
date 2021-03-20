#ifndef PACKET_TO_SERVER_H
#define PACKET_TO_SERVER_H

#include <stdint.h>
#include <endian.h>

#pragma pack(push, 1)
struct pkt_snake_update_t
{
//~ Packet UpdateOwnSnake
//~ The client sends this packet to the server when it receives a
//~ mouseMove, mouseDown, mouseUp, keyDown or keyUp event.
//~ Bytes 	Data type 	Value 	Description
//~ 0 	int8 	0-250 	mouseMove: the input angle. Clockwise, y-axes looks down the screen
//~ 0 	int8 	252 	keyDown, keyUp (left-arrow or right-arrow): turning left or right
//~ 0 	int8 	253 	mouseDown, keyDown (space or up-arrow): the snake is entering speed mode
//~ 0 	int8 	254 	mouseUp, keyUp (space or up-arrow): the snake is leaving speed mode
//~ 1 	int8 	0-255 	virtual (8ms) frames of counted rotation (0-127 left turns, 128-255 right turns, used when 1st byte is 252)
//~ angle in radians = 2pi * value / 250

	uint8_t angle;  // 0-250
	uint8_t key_down;
	uint8_t mouse_down;
	uint8_t mouse_up;
	uint8_t rot_count;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pkt_skin_t
{
	uint8_t pkt_type;  // 's' 0x73
	uint8_t protocol_id;  // 10 protocol_version - 1
	uint8_t skin_id;  // 0-38
	uint8_t nickname_len;
};
#pragma pack(pop)

#endif  // PACKET_TO_SERVER_H
