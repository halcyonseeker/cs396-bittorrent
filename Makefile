CC     = gcc
CFLAGS = -Wall -Wextra -Werror -Wpedantic -pthread -lcurl -lcrypto
TARGET = bitclient

# Required to link against the bencode library
GFLAGS = -fprofile-arcs -ftest-coverage

all: clean $(TARGET)

bitclient: magnet leecher seeder
	$(CC) $(CFLAGS) $(GFLAGS) -o $(TARGET) bitclient.c bencode/bencode.o magnet.o leecher.o seeder.o

magnet:
	$(CC) $(CFLAGS) $(GFLAGS) -o magnet.o -c magnet.c

leecher:
	$(CC) $(CFLAGS) $(GFLAGS) -o leecher.o -c leecher.c

seeder:
	$(CC) $(CFLAGS) $(GFLAGS) -o seeder.o -c seeder.c

clean:
	rm -f bitclient *.o *.gcda *.gcno vgcore.*
