CC     = gcc
CFLAGS = -Wall -Wextra -Werror -Wpedantic -pthread
TARGET = bitclient

# Required to link against the bencode library
GFLAGS = -fprofile-arcs -ftest-coverage

all: clean $(TARGET)

bitclient: extract
	$(CC) $(CFLAGS) $(GFLAGS) -o $(TARGET) bitclient.c bencode/bencode.o

extract:
	$(CC) $(CFLAGS) $(GFLAGS) -o extract.o -c extract.c

clean:
	rm -f bitclient *.o *.gcda *.gcno
