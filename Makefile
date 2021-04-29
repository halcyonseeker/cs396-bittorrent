CC     = gcc
CFLAGS = -Wall -Wextra -Werror -Wpedantic -pthread
TARGET = bitclient

# Required to link against the bencode library
GFLAGS = -fprofile-arcs -ftest-coverage

all: clean $(TARGET)

bitclient:
	$(CC) $(CFLAGS) $(GFLAGS) -o $(TARGET) bitclient.c bencode/bencode.o

clean:
	rm -f bitclient *.o *.gcda *.gcno
