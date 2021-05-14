#ifndef CLOCK_H
#define CLOCK_H

#include <cstdint>
#include <ctime>

#include <chrono>

static inline uint64_t uptime_us()
{
	using namespace std::chrono;
	struct timespec ts = {0, 0};
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return duration_cast<microseconds>(seconds{ts.tv_sec} + nanoseconds{ts.tv_nsec}).count();
}

uint64_t run_time_us();

#endif  // CLOCK_H
