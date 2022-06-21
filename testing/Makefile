CC = clang
CFLAGS = -Wall -pedantic -Werror -Wextra

HTTP = ../httpserver/
PRS_SRC = $(HTTP)parser.c parser_test.c
LIST_SRC = $(HTTP)ll.c $(HTTP)client.c list_test.c

.PHONY: all debug scan

all: Parser List

Parser: $(PRS_SRC) 
	$(CC) -o $@ $(PRS_SRC)

List: $(LIST_SRC) 
	$(CC) -o $@ $(LIST_SRC)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

format:
	clang-format -i -style=file *.[ch]

clean:
	rm -f *.o Parser List

scan: clean
	scan-build --use-cc=$(CC) make

debug: CFLAGS += -g
debug: clean all
