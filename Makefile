ZENROOM := $(if ${ZENROOM},${ZENROOM},$(shell realpath '../zenroom'))
CFLAGS := -I. -I ${ZENROOM}/src -O2 -ggdb -Wall -std=gnu99 -fPIC
ZENROOM_LIB := ${ZENROOM}/src/libzenroom-x86_64-0.10.so
LDADD := ${ZENROOM_LIB}
VERSION := $(shell cat ${ZENROOM}/VERSION)

SOURCES := \
	redroom.o


.c.o:
	$(CC) $(CFLAGS) -c $< -o $@ -DVERSION=\"${VERSION}\"

all: ${ZENROOM_LIB} ${SOURCES}
	${CC} ${CFLAGS} ${SOURCES} -shared ${LDADD} -o redroom.so

${ZENROOM_LIB}:
	make -C ${ZENROOM} linux-lib

clean:
	rm -f *.o *.so

