CC      ?= gcc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
LDLIBS  = -lm

all: friendrec evaluate

friendrec: src/main.c src/graph.c src/graph.h
	$(CC) $(CFLAGS) -o $@ src/main.c src/graph.c $(LDLIBS)

evaluate: src/evaluate.c src/graph.c src/graph.h
	$(CC) $(CFLAGS) -o $@ src/evaluate.c src/graph.c $(LDLIBS)

test_graph: tests/test_graph.c src/graph.c src/graph.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -g -o $@ tests/test_graph.c src/graph.c $(LDLIBS)

test: test_graph
	./test_graph

results/evaluation.json: evaluate
	./evaluate data/facebook_pages_edges.csv.gz 1000 10 > $@

eval: results/evaluation.json
	cat $<

clean:
	rm -f friendrec evaluate test_graph

.PHONY: all test eval clean
