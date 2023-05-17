all: main

CC = clang
override CFLAGS += --std=gnu99 -g -Wno-everything -I/Library/Developer/CommandLineTools/SDKs/MacOSX13.1.sdk/usr/include -pthread -lm

SRCS = $(shell find . -name '.ccls-cache' -type d -prune -o -type f -name '*.c' -print)
HEADERS = $(shell find . -name '.ccls-cache' -type d -prune -o -type f -name '*.h' -print)

main: clean $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(SRCS) -o "$@"

main-debug: $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -O0 $(SRCS) -o "$@"

# clean:
# 	rm -f main main-debug


SCHEDULER=controller.c
CCOPT=-g --std=gnu99
LIBS=-lpthread
BINARIES=main test1 test2 test3

# all: $(BINARIES)

# main: main.c controller.h $(SCHEDULER)
# 	@gcc $(CCOPT) main.c $(SCHEDULER) -o main $(LIBS) -Wno-implicit-function-declaration

# burn-in: clean controller.c
# 	gcc $(CCOPT) -Wno-implicit-function-declaration -DDELAY=1000 -DLOG_LEVEL=5 -DPASSENGERS=100 -DTRIPS_PER_PASSENGER=100 -DELEVATORS=10 -DFLOORS=40 main.c $(SCHEDULER) -o main $(LIBS)
# 	./main

test1: clean $(SCHEDULER)
	@gcc -DDELAY=10000   -DPASSENGERS=10   -DELEVATORS=1 -DFLOORS=6 -g main.c $(SCHEDULER) -o test1 $(LIBS) $(CCOPT) $(CFLAGS)

test2: clean $(SCHEDULER)
	@gcc -DDELAY=10000   -DFLOORS=2 -g --std=c99 main.c $(SCHEDULER) -o test2 $(LIBS) $(CCOPT)

test3: clean $(SCHEDULER)
	@gcc -DDELAY=10000   -DELEVATORS=10 -g --std=c99 main.c $(SCHEDULER) -o test3 $(LIBS) $(CCOPT)

clean:
	@rm -rf *~ *.dSYM $(BINARIES)

