ZENROOM := $(if ${ZENROOM},${ZENROOM},$(shell realpath '../../zenroom'))
REDIS   := $(if ${REDIS},${REDIS},$(shell realpath '../redis'))
CFLAGS := -Isrc -I ${ZENROOM}/src -O2 -ggdb -Wall -std=gnu99 -fPIC
LDFLAGS := -Wl,-Bsymbolic
ZENROOM_LIB := ${ZENROOM}/src/libzenroom-x86_64.so
LDADD := ${ZENROOM_LIB}
VERSION := $(shell cat ${ZENROOM}/VERSION)

SOURCES := \
	src/redroom.o src/exectokey.o src/setpwd.o


.c.o:
	$(CC) $(CFLAGS) -c $< -o $@ -DVERSION=\"${VERSION}\"

all: ${ZENROOM_LIB} ${SOURCES}
	${CC} ${CFLAGS} ${LDFLAGS} ${SOURCES} -shared ${LDADD} -o redroom.so

# benchmark
check: LDADD += ${REDIS}/deps/hiredis/libhiredis.a \
				${REDIS}/deps/jemalloc/lib/libjemalloc.a -lpthread -ldl
check: CFLAGS += -I${REDIS}/src -I${REDIS}/deps/hiredis \
				 -I${REDIS}/deps/jemalloc/include/jemalloc
check: ${ZENROOM_LIB} tests/benchmark.o
	${CC} ${CFLAGS} -I ${REDIS}/src tests/benchmark.o -o benchmark \
		${REDIS}/src/ae.o ${REDIS}/src/anet.o \
		${REDIS}/src/adlist.o ${REDIS}/src/zmalloc.o \
		${LDADD}

${ZENROOM_LIB}:
	make -C ${ZENROOM} linux-lib

clean:
	rm -f *.o *.so src/*.o src/*.so tests/*.o benchmark
