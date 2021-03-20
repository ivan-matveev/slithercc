#include "geometry.h"
#include "screen_sdl.h"
#include "log.h"

#include <cmath>
#include <deque>

float norm_angle(float angle)
{
	return fmod(angle + (M_PI * 2), M_PI * 2);
}

float xy_angle(xy_t p1, xy_t p2)
{
	float dx = p2.x - p1.x;
	float dy = p2.y - p1.y;
	float angle = ::atan2f(dy, dx);
	return norm_angle(angle);
}

xy_t xy_mid(xy_t xy1, xy_t xy2)
{
	return {(xy2.x + xy1.x) / 2, (xy2.y + xy1.y) / 2};
}

xy_t xy_extrapolate(xy_t xy1, xy_t xy2, coordinate_t dist)
{
	coordinate_t dist12 = distance(xy1, xy2);
	float ko = dist / static_cast<float>(dist12);
	return { xy2.x + (xy2.x - xy1.x) * ko,	xy2.y + (xy2.y - xy1.y) * ko};
}

xy_t xy_interpolate(xy_t xy1, xy_t xy2, coordinate_t dist)
{
	coordinate_t dist12 = distance(xy1, xy2);
	float ko = dist / static_cast<float>(dist12);
	return { xy1.x + (xy2.x - xy1.x) * ko,	xy1.y + (xy2.y - xy1.y) * ko};
}

void line_segment_append(std::deque<xy_t>& segment, const xy_t& p1, const xy_t& p2)
{
	coordinate_t dist = distance(p1, p2);
	for (ssize_t cnt = 0; cnt < dist; ++cnt)
		segment.emplace_back(
			xy_interpolate(p1, p2, cnt)
			);

}

std::deque<xy_t> make_line_segment(const xy_t& p1, const xy_t& p2)
{
	std::deque<xy_t> segment;
	line_segment_append(segment, p1, p2);
	return segment;
}

xy_t matr_mul_xy(const matr33_t& matr, const xy_t& xy)
{
	matr33_row_t xy3{xy.x, xy.y, 1.};
	matr33_row_t xy3_out{};
	for (size_t ir = 0; ir < 3; ++ir)
	{
		float row_sum = 0.;
		for (size_t ic = 0; ic < 3; ++ic)
		{
			row_sum += matr[ic][ir] * xy3[ic];
		}
		xy3_out[ir] = row_sum;
	}
	return xy_t{xy3_out[0], xy3_out[1]};
}

xy_t xy_rot(xy_t xy, xy_t center, float ang_rad)
{
	float cos = ::cosf(ang_rad);
	float sin = ::sinf(ang_rad);
	xy.x -= center.x;
	xy.y -= center.y;
	xy_t txy;
	txy.x = (xy.x * cos) - (xy.y * sin);
	txy.y = (xy.x * sin) + (xy.y * cos);
	xy.x = txy.x + center.x;
	xy.y = txy.y + center.y;
	return xy;
}

octagon_t make_octagon(const coordinate_t x, const coordinate_t y, coordinate_t radius)
{
	octagon_t oct{};
	xy_t ctr{x, y};
	float ang_stp = 2 * M_PI / oct.size();
	for (size_t cnt = 0; cnt < oct.size(); ++cnt)
	{
		oct[cnt] = ctr;
		oct[cnt].x += ::cosf(norm_angle(ang_stp * cnt)) * radius;
		oct[cnt].y += ::sinf(norm_angle(ang_stp * cnt)) * radius;
	}
	return oct;
}

octagon_t make_octastar(const coordinate_t x, const coordinate_t y, coordinate_t radius)
{
	octagon_t oct{
		xy_t{x + radius, y},
		xy_t{},
		xy_t{x, y + radius},
		xy_t{},
		xy_t{x - radius, y},
		xy_t{},
		xy_t{x, y - radius},
		xy_t{},
		};
	oct[1] = xy_mid(oct[0], oct[2]);
	oct[3] = xy_mid(oct[2], oct[4]);
	oct[5] = xy_mid(oct[4], oct[6]);
	oct[7] = xy_mid(oct[6], oct[0]);
	return oct;
}
