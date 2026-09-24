CXX = g++
CXXFLAGS = -Wall -Wextra -g

all: server client

server: server.cpp common.cpp hashtable.o server.h
	$(CXX) $(CXXFLAGS) -o server server.cpp common.cpp hashtable.o -lcrypto

client: client.cpp common.cpp hashtable.o server.h
	$(CXX) $(CXXFLAGS) -o client client.cpp common.cpp hashtable.o -lcrypto

hashtable.o: hashtable.cpp hashtable.h server.h
	$(CXX) $(CXXFLAGS) -c hashtable.cpp -o hashtable.o

clean:
	rm -f server client hashtable.o

.PHONY: all clean