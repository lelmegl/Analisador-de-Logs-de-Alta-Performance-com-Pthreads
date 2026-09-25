CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread
LDLIBS  = -pthread

all: seq par par_opt

seq: log_analyzer_seq.c
	$(CC) $(CFLAGS) log_analyzer_seq.c -o log_analyzer_seq $(LDLIBS)

par: log_analyzer_par.c log_common.h
	$(CC) $(CFLAGS) log_analyzer_par.c -o log_analyzer_par $(LDLIBS)

par_opt: log_analyzer_par_optimized.c log_common.h
	$(CC) $(CFLAGS) log_analyzer_par_optimized.c -o log_analyzer_par_optimized $(LDLIBS)

clean:
	rm -f log_analyzer_seq log_analyzer_par log_analyzer_par_optimized *.exe

.PHONY: all seq par par_opt clean
