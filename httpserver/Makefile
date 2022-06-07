CC = clang
CFLAGS = -Wall -pedantic -Werror -Wextra
SRC = httpserver.o parser.o response.o queue.o
LFLAGS = -pthread

.PHONY: all debug scan

all: httpserver

httpserver: $(SRC) 
	$(CC) $(LFLAGS) -o $@ $(SRC)

%.o: %.c
	$(CC) $(LFLAGS) $(CFLAGS) -c $<

format:
	clang-format -i -style=file *.[ch]

clean:
	rm -f *.o httpserver

scan: clean
	scan-build --use-cc=$(CC) make

debug: CFLAGS += -g
debug: clean all
