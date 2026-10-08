CXX      ?= g++
CXXFLAGS ?= -Wall -Wextra -Werror -std=c++17 -O2 -g -D_POSIX_C_SOURCE=200809L

TARGET   = server
SRCS     = server.cpp exec_job.cpp job_registry.cpp job_runner.cpp \
           ipc.cpp daemon.cpp client.cpp
OBJS     = $(SRCS:.cpp=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

test: $(TARGET)
	bash ../verif/scripts/run_tests.sh
