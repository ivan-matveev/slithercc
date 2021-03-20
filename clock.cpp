#include "clock.h"

uint64_t run_time_us()
{
	static uint64_t run_start = uptime_us();
	return uptime_us() - run_start;
}
