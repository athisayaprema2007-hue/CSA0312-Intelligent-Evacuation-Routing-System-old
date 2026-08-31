# Makefile - Intelligent Emergency Evacuation Routing System (CSA0312)
#
# Preferred build (modern GCC):   make
# On Windows without make, use build.bat instead.
#
# Note: on very old compilers (e.g. GCC 3.4.x shipped with Dev-C++),
# replace -std=c11 -Wpedantic with -std=c99 -pedantic; the code is
# written to compile cleanly under both.

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
BIN      = bin

CORE_SRC = src/graph.c src/hashmap.c src/hashset.c src/minheap.c \
           src/dijkstra.c src/route_cache.c

.PHONY: all test bench fail demo clean

all: $(BIN)/evacsim $(BIN)/test_runner

$(BIN)/evacsim: src/main.c $(CORE_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -o $@ src/main.c $(CORE_SRC)

$(BIN)/test_runner: tests/test_runner.c $(CORE_SRC)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -o $@ tests/test_runner.c $(CORE_SRC)

demo: $(BIN)/evacsim
	$(BIN)/evacsim demo

test: $(BIN)/test_runner
	$(BIN)/test_runner results/test_results.txt

bench: $(BIN)/evacsim
	$(BIN)/evacsim bench results/benchmark_results.csv

fail: $(BIN)/evacsim
	$(BIN)/evacsim fail results/road_failure_analysis.txt

clean:
	rm -rf $(BIN)
