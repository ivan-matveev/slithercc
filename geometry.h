#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <array>
#include <vector>
#include <deque>
#include <string>
#include <algorithm>
#include <cmath>

typedef int32_t coordinate_t;

struct xy_t
{
	coordinate_t x;
	coordinate_t y;
	bool operator == (const xy_t& other)
	{
		return x == other.x && y == other.y;
	}
	bool operator != (const xy_t& other)
	{
		return x != other.x || y != other.y;
	}
};

struct rect_t
{
	xy_t ul;
	xy_t lr;

	coordinate_t width(){ return lr.x - ul.x; }
	coordinate_t height(){ return lr.y - ul.y; }
	xy_t center(){ return xy_t{(lr.x + ul.x) / 2, (lr.y + ul.y) / 2}; }
	template <typename Txy>
	bool has(const Txy& xy) const { return rect_has_xy(*this, xy); }
};

template <typename Txy>
static inline bool rect_has_xy(const rect_t& rect, const Txy& xy)
{
	if (rect.ul.x > xy.x)
		return false;
	if (rect.ul.y > xy.y)
		return false;
	if (rect.lr.x < xy.x)
		return false;
	if (rect.lr.y < xy.y)
		return false;
	return true;
}

static inline coordinate_t distance(coordinate_t x1, coordinate_t y1, coordinate_t x2, coordinate_t y2)
{
	float dx2 = pow(x1 - x2, 2);
	float dy2 = pow(y1 - y2, 2);
	float distance = ::sqrtf(dx2 + dy2);
	return static_cast<size_t>(distance);
}

template <typename Txy1, typename Txy2>
static inline coordinate_t distance(const Txy1& xy1, const Txy2& xy2)
{
	return distance(xy1.x, xy1.y, xy2.x, xy2.y);
}

template <typename Txy>
static inline std::string to_str(const Txy& xy)
{
	return std::string(std::to_string(xy.x) + std::string(":") + std::to_string(xy.y));
}

template <typename Txy_list, typename Txy>
typename Txy_list::iterator
find_by_xy(Txy_list& xy_list, Txy xy)
{
	return std::find_if(
		xy_list.begin(), xy_list.end(),
		[xy](const typename Txy_list::value_type& elem)
		{ return elem.x == xy.x && elem.y == xy.y; }
		);
}

template <size_t N_SEG>
std::vector<xy_t> make_quad_bezier(xy_t p1, xy_t p2, xy_t p3)
{
	std::vector<xy_t> pts(N_SEG + 1);
	for (size_t idx = 0; idx < N_SEG + 1; ++idx)
	{
		float t = static_cast<float>(idx) / static_cast<float>(N_SEG);
		float a = pow((1.0 - t), 2.0);
		float b = 2.0 * t * (1.0 - t);
		float c = pow(t, 2.0);
		pts[idx].x = a * p1.x + b * p2.x + c * p3.x;
		pts[idx].y = a * p1.y + b * p2.y + c * p3.y;
	}
	return pts;
}


typedef std::array<float,        3> matr33_row_t;
typedef std::array<matr33_row_t, 3> matr33_t;

typedef std::array<xy_t, 8> octagon_t;

xy_t matr_mul_xy(const matr33_t& matr, const xy_t& xy);

float norm_angle(float angle);
float xy_angle(xy_t p1, xy_t p2);
xy_t xy_rot(xy_t xy, xy_t center, float ang_rad);
xy_t xy_mid(xy_t xy1, xy_t xy2);
xy_t xy_extrapolate(xy_t xy1, xy_t xy2, coordinate_t dist);
xy_t xy_interpolate(xy_t xy1, xy_t xy2, coordinate_t dist);

octagon_t make_octastar(const coordinate_t x, const coordinate_t y, coordinate_t radius);
octagon_t make_octagon(const coordinate_t x, const coordinate_t y, coordinate_t radius);
void line_segment_append(std::deque<xy_t>& segment, const xy_t& p1, const xy_t& p2);
std::deque<xy_t> make_line_segment(const xy_t& p1, const xy_t& p2);
template <size_t N_SEG>
std::vector<xy_t> make_quad_bezier(xy_t p1, xy_t p2, xy_t p3);

#endif  // GEOMETRY_H
