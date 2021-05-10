CXX      = clang++
CXXFLAGS = -Wall -Wextra -Werror -Wpedantic -ltorrent-rasterbar
TARGET   = bitclient-lt

all: clean $(TARGET)

bitclient-lt:
	$(CXX) $(CXXFLAGS) -o $(TARGET) bitclient-lt.cpp

clean:
	rm -f bitclient-lt *.o vgcore.*
