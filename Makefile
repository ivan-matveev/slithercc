ZLIB_LDFLAGS:=$(shell pkg-config --libs zlib)

BOOST_VER := 1.67.0
BOOST := $(subst .,_,${BOOST_VER})
BOOST := boost_${BOOST}
BOOST_LIB_LIST := ${BOOST}_install/lib/libboost_iostreams.a ${BOOST}_install/lib/libboost_system.a

CXX_FLAGS += -Wall -Wextra -g -DRUN_TIME -Wno-narrowing -Wstrict-aliasing=3
CXX_FLAGS += -isystem ${BOOST}_install/include
CXX_FLAGS += -DNDEBUG
# CXX_FLAGS += -DDEBUG_LOG
CXX_FLAGS += -std=c++11

.PHONY: all clean

all: slithercc slithercc_replay_server

SLITHERCC_OBJ_LIST := slithercc_boost.o game.o websocket_boost.o connect.o \
	http_get.o util.o ioc.o geometry.o decode_secret.o clock.o

slithercc: ${SLITHERCC_OBJ_LIST} ${BOOST_LIB_LIST}
	${CXX} \
		-lpthread \
		-lSDL2 \
		-lSDL2_ttf \
		${ZLIB_LDFLAGS} \
		${CXX_FLAGS} \
		-o $@ $^

slithercc_replay_server: slithercc_replay_server.o clock.o util.o ${BOOST_LIB_LIST}
	${CXX} \
		-lpthread \
		${CXX_FLAGS} \
		-o $@ $^

%.o: %.cpp | ${BOOST_LIB_LIST}
	${CXX} ${CXX_FLAGS} -MMD -MP -c $<

${BOOST}.tar.bz2:
	wget https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VER}/source/${BOOST}.tar.bz2

${BOOST_LIB_LIST}: ${BOOST}.tar.bz2
	tar xf ${BOOST}.tar.bz2; \
	mkdir -p ${BOOST}_install; \
	cd ${BOOST}; \
	./bootstrap.sh; \
	./b2 install --prefix=../${BOOST}_install --with-system --with-iostreams

clean:
	rm *.o *.d slithercc slithercc_replay_server core || true

deps = $(wildcard *.d)
-include $(deps)
