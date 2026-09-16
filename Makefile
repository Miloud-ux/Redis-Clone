CXX = g++
CXXFLAGS = -Wall -Wextra -g

all: server client

server: server.cpp common.cpp server.h
	$(CXX) $(CXXFLAGS) -o server server.cpp common.cpp

client: client.cpp common.cpp server.h
	$(CXX) $(CXXFLAGS) -o client client.cpp common.cpp

# library with no main() yet; built as an object to link into the server later
hashtable.o: hashtable.cpp hashtable.h
	$(CXX) $(CXXFLAGS) -c hashtable.cpp -o hashtable.o

clean:
	rm -f server client hashtable.o

.PHONY: all clean
