CC = gcc
CFLAGS = -Wall -pthread

all: seq

seq: log_analyzer_seq.c
	$(CC) $(CFLAGS) log_analyzer_seq.c -o log_analyzer_seq

clean:
	rm -f log_analyzer_seq log_analyzer_par