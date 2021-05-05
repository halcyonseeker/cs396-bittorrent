CC     = gcc
CFLAGS = -Wall -Wextra -Werror -Wpedantic -pthread -lcurl -lcrypto
TARGET = bitclient

# Required to link against the bencode library
GFLAGS = -fprofile-arcs -ftest-coverage

all: clean $(TARGET)

bitclient: magnet
	$(CC) $(CFLAGS) $(GFLAGS) -o $(TARGET) bitclient.c bencode/bencode.o magnet.o

magnet:
	$(CC) $(CFLAGS) $(GFLAGS) -o magnet.o -c magnet.c

# tracker:
# 	$(CC) $(CFLAGS) $(GFLAGS) -o tracker.o -c tracker.c

clean:
	rm -f bitclient *.o *.gcda *.gcno vgcore.*
