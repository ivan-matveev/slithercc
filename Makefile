ZLIB_LDFLAGS:=$(shell pkg-config --libs zlib)

CXX_FLAGS += -Wall -Wextra -g -DRUN_TIME -Wno-narrowing
CXX_FLAGS += -DNDEBUG
# CXX_FLAGS += -DDEBUG_LOG

.PHONY: all clean

all: slithercc slithercc_replay_server

slithercc: slithercc_boost.o game.o websocket_boost.o connect.o http_get.o util.o ioc.o geometry.o decode_secret.o clock.o
	${CXX} \
		-lboost_system \
		-lboost_iostreams \
		-lpthread \
		-lSDL2 \
		-lSDL2_ttf \
		${ZLIB_LDFLAGS} \
		${CXX_FLAGS} \
		-o $@ $^

slithercc_replay_server: slithercc_replay_server.o clock.o util.o
	${CXX} \
		-lboost_system \
		-lboost_iostreams \
		-lpthread \
		${CXX_FLAGS} \
		-o $@ $^

%.o: %.cpp
	${CXX} ${CXX_FLAGS} -MMD -MP -c $<

clean:
	rm *.o *.d slithercc slithercc_replay_server core || true

deps = $(wildcard *.d)
-include $(deps)
