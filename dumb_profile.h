#ifndef DUMBPROFILE_H_
#define DUMBPROFILE_H_

#include "clock.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define CONCAT_HELP(X1, X2) X1##X2
#define CONCAT(X1, X2) CONCAT_HELP(X1, X2)
#define DPROFILE dumb_profile CONCAT(dprf, __LINE__) = dumb_profile(__FUNCTION__, __LINE__); CONCAT(dprf, __LINE__).start();

// void uptime_tv(struct timeval* tv);

// #ifndef UPTIME_US_H_
// #define UPTIME_US_H_
// static inline uint64_t uptime_us()
// {
// 	struct timespec uptime_ts;
// 	bzero(&uptime_ts, sizeof(uptime_ts));
// 	clock_gettime(CLOCK_MONOTONIC_RAW, &uptime_ts);
// 	int64_t uptime_ns = 0;
// 	uptime_ns  = (int64_t)uptime_ts.tv_sec  * 1000000000UL;
// 	uptime_ns +=          uptime_ts.tv_nsec;
// 	int64_t uptime_us = uptime_ns / 1000;
// 	return uptime_us;
// }
// #endif  // #ifndef UPTIME_US_H_


struct dumb_profile
{
    uint64_t start_;
    uint64_t end_;
    const char* fname_;
    const uint32_t lnum_;

    dumb_profile(const char* fname, const uint32_t lnum)
        : start_(0)
        , end_(0)
        , fname_(fname)
        , lnum_(lnum)
    {
    }

    ~dumb_profile()
    {
        stop();
    }

    void start()
    {
        start_ = uptime_us();
    }

    void stop()
    {
        print();
        memset(&start_, 0, sizeof(start_));
        memset(&end_, 0, sizeof(end_));
    }

    int64_t started()
    {
        return start_;
    }

    void print()
    {
        end_ = uptime_us();
        uint64_t delta = end_ - start_;
#if __WORDSIZE == 64
        printf("profile: %s:%d: now:%lu elapsed:%lu us\n",
			fname_, lnum_,
			end_, delta);
#else
        printf("profile: %s:%d: now:%llu elapsed:%llu us\n",
			fname_, lnum_,
			end_, delta);
#endif
    }
};

#endif //DUMBPROFILE_H_
