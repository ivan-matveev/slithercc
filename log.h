#ifndef LOG_H
#define LOG_H

#include "clock.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef RUN_TIME

#ifdef DEBUG_LOG
#define LOG(format, ...) do{ \
				printf( \
					"[%06lu] %s:%d:" format "\n", \
					run_time_us() / 1000, \
					__FUNCTION__, \
					__LINE__, \
					##__VA_ARGS__); \
				}while(0)
#else
#define LOG(...)
#endif  // DEBUG_LOG

#define ERR(format, ...) do{ \
				fprintf(stderr, \
					"[%06lu] %s:%d:" format "\n", \
					run_time_us() / 1000, \
					__FUNCTION__, \
					__LINE__, \
					##__VA_ARGS__); \
				}while(0)
#else  // RUN_TIME

#ifdef DEBUG_LOG
#define LOG(format, ...) do{ \
				printf( \
					"[%06lu] %s:%d:" format "\n", \
					uptime_us() / 1000, \
					__FUNCTION__, \
					__LINE__, \
					##__VA_ARGS__); \
				}while(0)
#else
#define LOG(...)
#endif  // DEBUG_LOG

#define ERR(format, ...) do{ \
				fprintf(stderr, \
					"[%06lu] %s:%d:" format "\n", \
					uptime_us() / 1000, \
					__FUNCTION__, \
					__LINE__, \
					##__VA_ARGS__); \
				}while(0)
#endif  // RUN_TIME

#ifdef __cplusplus
#include <iostream>

#ifdef DEBUG_LOG
#define LOGCC std::cout << __FUNCTION__ << ":" << __LINE__ << ": "
#define ERRCC std::cerr << __FUNCTION__ << ":" << __LINE__ << ": "
#else
struct null_out_t
{
	template <typename T>
	null_out_t& operator << (T arg)
	{ (void)arg; return *this; }
	null_out_t& operator<<(std::ostream& (*m)(std::ostream&))
	{ (void)m; return *this; }
};

static inline null_out_t& null_out_f()
{
	static null_out_t null_out;
	return null_out;
}

#define LOGCC null_out_f() << __FUNCTION__ << ":" << __LINE__ << ": "
#define ERRCC null_out_f() << __FUNCTION__ << ":" << __LINE__ << ": "
#endif  // DEBUG_LOG

static inline void print_hex(const void* buf_, size_t size)
{
	const uint8_t* buf = reinterpret_cast<const uint8_t*> (buf_);
	for (const uint8_t* it = buf; it < buf + size; ++it)
	{
		printf("%02x ", *it);
	}
	printf("\n");
}

#endif  // __cplusplus

#endif  // LOG_H
