CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17

all: server

server: server.cpp job.h
	$(CXX) $(CXXFLAGS) -o server server.cpp

clean:
	rm -f server
