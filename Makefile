CXX = g++
CXXFLAGS = -Wall -Wextra -g

all: server client

server: server.cpp common.cpp server.h
	$(CXX) $(CXXFLAGS) -o server server.cpp common.cpp

client: client.cpp common.cpp server.h
	$(CXX) $(CXXFLAGS) -o client client.cpp common.cpp

clean:
	rm -f server client

.PHONY: all clean
