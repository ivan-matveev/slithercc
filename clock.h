#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

//fix for gcc 4.3
#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW		4
#endif

static inline uint64_t clock_gettime_us(clockid_t clk_id)
{
	struct timespec ts = {0, 0};
	clock_gettime(clk_id, &ts);
	int64_t uptime_ns = 0;
	uptime_ns  = (int64_t)ts.tv_sec  * 1000000000UL;
	uptime_ns +=          ts.tv_nsec;
	int64_t uptime_us = uptime_ns / 1000;
	return uptime_us;
}

inline uint64_t uptime_us()
{
	return clock_gettime_us(CLOCK_MONOTONIC_RAW);
}

inline uint64_t realtime_us()
{
	return clock_gettime_us(CLOCK_REALTIME);
}

uint64_t run_time_us();

#ifdef __cplusplus
}
#endif


#endif  // CLOCK_H
