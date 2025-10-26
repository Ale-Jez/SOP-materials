CC=gcc
C_FLAGS=-Wall -g -Wno-unused
L_FLAGS=-lrt -fsanitize=address,undefined

TARGET=part3
FILES=${TARGET}.o

.PHONY: clean all

${TARGET} : ${FILES}
	${CC} ${L_FLAGS} -o ${TARGET} ${FILES}

${TARGET}.o: ${TARGET}.c
	${CC} ${C_FLAGS} -o ${TARGET}.o -c ${TARGET}.c

all: ${TARGET}

clean:
	rm -f ${FILES} ${TARGET} part1 part2 part3 part4 2>/dev/null