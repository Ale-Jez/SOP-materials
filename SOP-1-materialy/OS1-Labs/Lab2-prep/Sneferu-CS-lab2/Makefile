override CFLAGS=-Wall -Wextra -Wshadow -Wno-unused-parameter -g -O0 -fsanitize=address,undefined

ifdef CI
override CFLAGS=-Wall -Wextra -Wshadow -Werror -Wno-unused-parameter
endif

NAME=ops-sneferu

.PHONY: clean all

all: ${NAME}

${NAME}: ${NAME}.c
	gcc $(CFLAGS) -o ${NAME} ${NAME}.c

clean:
	rm -f ${NAME}
