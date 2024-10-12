#ifndef SCREEN_SDL_H
#define SCREEN_SDL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "log.h"
#include "geometry.h"

#include <cassert>
#include <unordered_map>

struct screen_sdl_t
{
	explicit screen_sdl_t(coordinate_t width_, coordinate_t height_, std::string font_path)
	: width(width_)
	, height(height_)
	, window()
	, renderer()
	, event()
	, font_path(font_path)
	, x_prev(0)
	, y_prev(0)
	, quit(false)
	, text_texture_map()
	, circle_radius_texture_map()
	, octastar_radius_texture_map()
	, octagon_radius_texture_map()
	{
		SDL_Init(SDL_INIT_VIDEO);
		SDL_SetEventFilter(event_filter, this);
		window = SDL_CreateWindow("",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			width, height, 0);
		assert(window != nullptr);
		if (width == 0 || height == 0)
		{
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			SDL_GetWindowSize(window, &width, &height);
		}
		renderer = SDL_CreateRenderer(window, -1, 0);
		assert(renderer != nullptr);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		TTF_Init();
		SDL_RenderClear(renderer);
	}

	~screen_sdl_t()
	{
		for (auto const& twh : text_texture_map)
			SDL_DestroyTexture(twh.second.texture);
		for (auto texture : circle_radius_texture_map)
			if (texture != nullptr)
				SDL_DestroyTexture(texture);
		for (auto texture : octastar_radius_texture_map)
			if (texture != nullptr)
				SDL_DestroyTexture(texture);
		for (auto texture : octagon_radius_texture_map)
			if (texture != nullptr)
				SDL_DestroyTexture(texture);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	void window_title(const char* title)
	{
		SDL_SetWindowTitle(window, title);
	}

	void clear()
	{
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderClear(renderer);
	}

	struct color_t
	{
		color_t()
		: r(0)
		, g(0)
		, b(0)
		{
		}

		color_t(uint8_t r_, uint8_t g_, uint8_t b_)
		: r(r_)
		, g(g_)
		, b(b_)
		{
		}

		bool operator == (const color_t& other) const
		{
			return (
				r == other.r &&
				g == other.g &&
				b == other.b
				);
		}

		bool operator != (const color_t& other) const
		{
			return ! (*this == other);
		}

		uint8_t r;
		uint8_t g;
		uint8_t b;
	};

	void plot(coordinate_t x, coordinate_t y, screen_sdl_t::color_t color)
	{
		if (color_prev != color)
		{
			x_prev = 0;
			y_prev = 0;
		}
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
		coordinate_t x_sdl = x;
		coordinate_t y_sdl = (height - y) / 2;
		SDL_RenderDrawLine(renderer, x_sdl, y_sdl, x_prev, y_prev);
		x_prev = x_sdl;
		y_prev = y_sdl;
		color_prev = color;
	}

	void point(coordinate_t x, coordinate_t y, screen_sdl_t::color_t color)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
		SDL_RenderDrawPoint(renderer, x, y);
	}

	void line(coordinate_t x, coordinate_t y, coordinate_t x1, coordinate_t y1, screen_sdl_t::color_t color)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
		SDL_RenderDrawLine(renderer, x, y, x1, y1);
	}

	void rect(coordinate_t x, coordinate_t y, coordinate_t x1, coordinate_t y1, screen_sdl_t::color_t color)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
		const int width = x1 - x;
		const int height = y1 - y;
		const SDL_Rect rect{x, y, width, height};
		SDL_RenderDrawRect(renderer, &rect);
	}

	void circle(coordinate_t x, coordinate_t y, coordinate_t radius, screen_sdl_t::color_t color)
	{
		coordinate_t width_tmp = radius * 2 + 2;
		coordinate_t height_tmp = radius * 2 + 2;
		SDL_Rect rect = {x - width_tmp/2, y - height_tmp/2, width_tmp, height_tmp};
		SDL_Texture* texture = nullptr;
		if (circle_radius_texture_map[radius] != nullptr)
		{
			texture = circle_radius_texture_map[radius];
			SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
			SDL_RenderCopy(renderer, texture, nullptr, &rect);
			return;
		}

		SDL_Surface* surface = SDL_CreateRGBSurface(
			SDL_SWSURFACE,
			width_tmp, height_tmp,
			32, 0, 0, 0, 0);
		assert(surface);
		SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0, 0, 0));
		SDL_Renderer* renderer_sw = SDL_CreateSoftwareRenderer(surface);
		assert(renderer_sw);
		SDL_SetRenderDrawColor(renderer_sw, 0, 0, 0, 0);
		SDL_RenderClear(renderer_sw);

		size_t circle_len = 2 * M_PI * radius;
		float angle_step = 2 * M_PI / circle_len;
		SDL_SetRenderDrawColor(renderer_sw, 255, 255, 255, 255);
		for (size_t cnt = 0; cnt < circle_len; ++cnt)
		{
			xy_t xy{radius, 0};
			float angle = angle_step * cnt;
			xy = xy_rot(xy, xy_t{width_tmp/2, width_tmp/2}, angle);
			SDL_RenderDrawPoint(renderer_sw, xy.x, xy.y);
		}
		texture = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_DestroyRenderer(renderer_sw);
		SDL_FreeSurface(surface);
		circle_radius_texture_map[radius] = texture;
		SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
		SDL_RenderCopy(renderer, texture, nullptr, &rect);
	}

	void octastar(coordinate_t x, coordinate_t y, coordinate_t radius, screen_sdl_t::color_t color)
	{
		coordinate_t width_tmp = radius * 2 + 2;
		coordinate_t height_tmp = radius * 2 + 2;
		SDL_Rect rect = {x - width_tmp/2, y - height_tmp/2, width_tmp, height_tmp};
		SDL_Texture* texture = nullptr;
		if (octastar_radius_texture_map[radius] != nullptr)
		{
			texture = octastar_radius_texture_map[radius];
			SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
			SDL_RenderCopy(renderer, texture, nullptr, &rect);
			return;
		}

		SDL_Surface* surface = SDL_CreateRGBSurface(
			SDL_SWSURFACE,
			width_tmp, height_tmp,
			32, 0, 0, 0, 0);
		assert(surface);
		SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0, 0, 0));
		SDL_Renderer* renderer_sw = SDL_CreateSoftwareRenderer(surface);
		assert(renderer_sw);
		SDL_SetRenderDrawColor(renderer_sw, 0, 0, 0, 0);
		SDL_RenderClear(renderer_sw);
		SDL_SetRenderDrawColor(renderer_sw, 255, 255, 255, 255);
		octagon_t oct = make_octastar(width_tmp/2, width_tmp/2, radius);
		SDL_RenderDrawLine(renderer_sw,width_tmp/2, width_tmp/2, oct[0].x, oct[0].y);
		SDL_RenderDrawLine(renderer_sw,width_tmp/2, width_tmp/2, oct[1].x, oct[1].y);
		SDL_RenderDrawLine(renderer_sw,width_tmp/2, width_tmp/2, oct[2].x, oct[2].y);
		SDL_RenderDrawLine(renderer_sw,width_tmp/2, width_tmp/2, oct[3].x, oct[3].y);
		SDL_RenderDrawLine(renderer_sw,width_tmp/2, width_tmp/2, oct[4].x, oct[4].y);
		SDL_RenderDrawLine(renderer_sw,width_tmp/2, width_tmp/2, oct[5].x, oct[5].y);
		SDL_RenderDrawLine(renderer_sw,width_tmp/2, width_tmp/2, oct[6].x, oct[6].y);
		SDL_RenderDrawLine(renderer_sw,width_tmp/2, width_tmp/2, oct[7].x, oct[7].y);
		texture = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_DestroyRenderer(renderer_sw);
		SDL_FreeSurface(surface);
		octastar_radius_texture_map[radius] = texture;
		SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
		SDL_RenderCopy(renderer, texture, nullptr, &rect);
	}

	void octagon(coordinate_t x, coordinate_t y, coordinate_t radius, screen_sdl_t::color_t color)
	{
		coordinate_t width_tmp = radius * 2 + 2;
		coordinate_t height_tmp = radius * 2 + 2;
		SDL_Rect rect = {x - width_tmp/2, y - height_tmp/2, width_tmp, height_tmp};
		SDL_Texture* texture = nullptr;
		if (octagon_radius_texture_map[radius] != nullptr)
		{
			texture = octagon_radius_texture_map[radius];
			SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
			SDL_RenderCopy(renderer, texture, nullptr, &rect);
			return;
		}

		SDL_Surface* surface = SDL_CreateRGBSurface(
			SDL_SWSURFACE,
			width_tmp, height_tmp,
			32, 0, 0, 0, 0);
		assert(surface);
		SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0, 0, 0));
		SDL_Renderer* renderer_sw = SDL_CreateSoftwareRenderer(surface);
		assert(renderer_sw);
		SDL_SetRenderDrawColor(renderer_sw, 0, 0, 0, 0);
		SDL_RenderClear(renderer_sw);
		SDL_SetRenderDrawColor(renderer_sw, 255, 255, 255, 255);
		octagon_t oct = make_octagon(width_tmp/2, width_tmp/2, radius);
		SDL_RenderDrawLine(renderer_sw, oct[0].x, oct[0].y, oct[1].x, oct[1].y);
		SDL_RenderDrawLine(renderer_sw, oct[1].x, oct[1].y, oct[2].x, oct[2].y);
		SDL_RenderDrawLine(renderer_sw, oct[2].x, oct[2].y, oct[3].x, oct[3].y);
		SDL_RenderDrawLine(renderer_sw, oct[3].x, oct[3].y, oct[4].x, oct[4].y);
		SDL_RenderDrawLine(renderer_sw, oct[4].x, oct[4].y, oct[5].x, oct[5].y);
		SDL_RenderDrawLine(renderer_sw, oct[5].x, oct[5].y, oct[6].x, oct[6].y);
		SDL_RenderDrawLine(renderer_sw, oct[6].x, oct[6].y, oct[7].x, oct[7].y);
		SDL_RenderDrawLine(renderer_sw, oct[7].x, oct[7].y, oct[0].x, oct[0].y);
		texture = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_DestroyRenderer(renderer_sw);
		SDL_FreeSurface(surface);
		octagon_radius_texture_map[radius] = texture;
		SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
		SDL_RenderCopy(renderer, texture, nullptr, &rect);
	}

	void text(coordinate_t x, coordinate_t y, screen_sdl_t::color_t color, coordinate_t size, const char* text)
	{
		assert(text);
		static TTF_Font* font = TTF_OpenFont(font_path.c_str(), size);
		assert(font);
		std::string text_str = text;
		auto it = text_texture_map.find(std::string(text_str));
		if (it != text_texture_map.end())
		{
			texture_width_height_t* twh = &it->second;
			SDL_Rect renderQuad = { x, y, twh->width, twh->height };
			SDL_SetTextureColorMod(twh->texture, color.r, color.g, color.b);
			SDL_RenderCopy(renderer, twh->texture, NULL, &renderQuad);
			return;
		}

		SDL_Color textColor = { 255, 255, 255, 0 };
		SDL_Surface* surface = TTF_RenderText_Solid(font, text, textColor);
		assert(surface);
		SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0, 0, 0));
		SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
		int text_width = surface->w;
		int text_height = surface->h;
		SDL_FreeSurface(surface);

		SDL_Rect renderQuad = { x, y, text_width, text_height };
		SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
		SDL_RenderCopy(renderer, texture, NULL, &renderQuad);
		texture_width_height_t twh{texture, text_width, text_height};
		text_texture_map.insert(std::make_pair(text_str, twh));
	}

	void window_position(coordinate_t& x, coordinate_t& y)
	{
		SDL_GetWindowPosition(window, &x, &y);
	}
	bool mouse_position(coordinate_t& x, coordinate_t& y)
	{
		SDL_PumpEvents();
		SDL_GetMouseState(&x,&y);
// TODO think: somehow SDL get coordinates inside the window
// 		coordinate_t win_x;
// 		coordinate_t win_y;
// 		SDL_GetWindowPosition(window, &win_x, &win_y);
// 		x -= win_x;
// 		y -= win_y;
// 		LOG("win:%d:%d mos:%d:%d ", win_x, win_x, x, y);
// 		if (x < 0 || y < 0)
// 			return false;
// 		if (x > width || y > height)
// 			return false;
		return true;
	}

	bool mouse_button_left()
	{
		SDL_PumpEvents();
		uint32_t button_mask = SDL_GetMouseState(NULL, NULL);
		return button_mask & SDL_BUTTON(SDL_BUTTON_LEFT);
	}

	static int event_filter(void* userdata,  SDL_Event* event)
    {
		screen_sdl_t* thiz = reinterpret_cast<screen_sdl_t*>(userdata);
		if (event->type == SDL_QUIT)
			thiz->quit = true;
		if (event->type == SDL_KEYDOWN &&
			event->key.keysym.sym == SDLK_ESCAPE)
			thiz->quit = true;
		return 1; // add event to SDL event queue
	}

	void present()
	{
		SDL_RenderPresent(renderer);
	}

	void show()
	{
		quit = false;

		SDL_RenderPresent(renderer);
		while (true)
		{
			if (!SDL_WaitEvent(&event))
				continue;
			if (event.type == SDL_QUIT)
			{
				quit = true;
				break;
			}
			if (event.type == SDL_KEYDOWN &&
				event.key.keysym.sym == 'q')
			{
				quit = true;
				break;
			}
			if (event.type == SDL_KEYDOWN &&
				event.key.keysym.sym == 'n')
				break;
		}
	}

	void wait_any_key()
	{
		SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
		while (true)
		{
			SDL_PollEvent(&event);
			if (event.type == SDL_QUIT ||
				event.type == SDL_KEYDOWN ||
				event.type == SDL_MOUSEBUTTONDOWN
			)
			break;
		}
		LOG("event.type:%d", event.type);
	}

	coordinate_t width;
	coordinate_t height;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Event event;
	std::string font_path;
	coordinate_t x_prev;
	coordinate_t y_prev;
	color_t color_prev;
	bool quit;
	struct texture_width_height_t
	{
		SDL_Texture* texture;
		coordinate_t width;
		coordinate_t height;
	};
	std::unordered_map<std::string, texture_width_height_t> text_texture_map;
	std::array<SDL_Texture*, 1024> circle_radius_texture_map;
	std::array<SDL_Texture*, 1024> octastar_radius_texture_map;
	std::array<SDL_Texture*, 1024> octagon_radius_texture_map;
};

#endif  // SCREEN_SDL_H
