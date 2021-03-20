#ifndef TIMERFD_GRID_H
#define TIMERFD_GRID_H

#include "clock.h"
#include "log.h"

#include <sys/timerfd.h>
#include <errno.h>
#include <unistd.h>

inline int timerfd_grid_us_create(const uint32_t& grid_step_us)
{
	int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (timerfd == -1)
	{
		ERR("failed: timerfd_create(): %s", strerror(errno));
		return -1;
	}

	itimerspec its = {};
	its.it_interval.tv_nsec = grid_step_us * 1000;
	its.it_value.tv_nsec = grid_step_us * 1000;
	if (timerfd_settime(timerfd, 0, &its, NULL) != 0)
	{
		close(timerfd);
		ERR("failed: timerfd_settime(): %s", strerror(errno));
		return -1;
	}
	LOG("grid_step_us:%u timerfd:%d", grid_step_us, timerfd);
	return timerfd;
}

inline bool timerfd_wait(int timerfd)
{
	assert(timerfd != -1);
	uint64_t expired_count = 0;
	ssize_t read_count = read(timerfd, &expired_count, sizeof(expired_count));
	bool ret_val = true;
	if (read_count == -1)
	{
		ERR("read_count == -1 : %s", strerror(errno));
		ret_val = false;
	}
	if (expired_count != 1)
	{
		LOG("expired_count:%ju", expired_count);
		ret_val = false;
	}

	return ret_val;
}

#ifdef __cplusplus
template <std::size_t GRID_STEP_US>
bool timerfd_grid_us_wait()
{
	static const uint32_t grid_step_us = GRID_STEP_US;
	thread_local int timerfd = timerfd_grid_us_create(grid_step_us);
	return timerfd_wait(timerfd);
}
#endif

#endif  // TIMERFD_GRID_H
