CC=gcc
CFLAGS=-std=gnu99 -Wall -fsanitize=address,undefined
LDFLAGS=-fsanitize=address,undefined
LDLIBS=-lpthread -lm

TARGET=task1
FILES=${TARGET}.o

.PHONY: clean all

${TARGET} : ${FILES}
	${CC} ${LDFLAGS} ${LDLIBS} -o ${TARGET} ${FILES}

${TARGET}.o: ${TARGET}.c
	${CC} ${CFLAGS} -o ${TARGET}.o -c ${TARGET}.c

all: ${TARGET}

clean:
	rm -f ${FILES} ${TARGET}
