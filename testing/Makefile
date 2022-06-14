CC = clang
CFLAGS = -Wall -pedantic -Werror -Wextra
PRS_SRC = ../httpserver/parser.c parser_test.c
LFLAGS = -pthread

.PHONY: all debug scan

all: Parser

Parser: $(PRS_SRC) 
	$(CC) $(LFLAGS) -o $@ $(PRS_SRC)

%.o: %.c
	$(CC) $(LFLAGS) $(CFLAGS) -c $<

format:
	clang-format -i -style=file *.[ch]

clean:
	rm -f *.o Parser

scan: clean
	scan-build --use-cc=$(CC) make

debug: CFLAGS += -g
debug: clean all
