#ifndef PROFILE_H
#define PROFILE_H

#include <chrono>

struct profile_t
{
	using namespace std::chrono;
	profile_t()
	: start(steady_clock::now())
	{}
	~profile_t(){}
	size_t elapsed_ms()
	{
		return duration_cast<microseconds>steady_clock::now() - start;
	}
	
	steady_clock::time_point start;
	const char* name;
	size_t line;
};

#endif
