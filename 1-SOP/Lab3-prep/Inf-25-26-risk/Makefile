override CFLAGS=-std=c17 -Wall -Wextra -Wshadow -Wno-unused-parameter -Wno-unused-const-variable -g -pthread -O0 -fsanitize=address,undefined

ifdef CI
override CFLAGS=-std=c17 -Wall -Wextra -Wshadow -Werror -Wno-unused-parameter -Wno-unused-const-variable -pthread
endif

NAME=sop-risk-stage4

.PHONY: clean all

all: ${NAME}

${NAME}: ${NAME}.c
	$(CC) $(CFLAGS) -o ${NAME} ${NAME}.c

clean:
	rm -f ${NAME}
