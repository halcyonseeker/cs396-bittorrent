CC     = gcc
CFLAGS = -Wall -Wextra -Werror -Wpedantic -pthread -lcurl
TARGET = bitclient

# Required to link against the bencode library
GFLAGS = -fprofile-arcs -ftest-coverage

all: clean $(TARGET)

bitclient: extract tracker
	$(CC) $(CFLAGS) $(GFLAGS) -o $(TARGET) bitclient.c bencode/bencode.o extract.o tracker.o

extract:
	$(CC) $(CFLAGS) $(GFLAGS) -o extract.o -c extract.c

tracker:
	$(CC) $(CFLAGS) $(GFLAGS) -o tracker.o -c tracker.c

clean:
	rm -f bitclient *.o *.gcda *.gcno
