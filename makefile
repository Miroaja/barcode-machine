# Compiler
CXX = clang++

CXXFLAGS = -O3 -std=c++23 -Wall -Wextra -Werror
LINKFLAGS = -lssl -lcrypto

SERVER_SRCS = server_main.cpp
CLIENT_SRCS = client_main.cpp

SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.cpp=.o)

SERVER_TARGET = server
CLIENT_TARGET = client
all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(LINKFLAGS) $(SERVER_OBJS) -o $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CXX) $(LINKFLAGS) $(CLIENT_OBJS) -o $(CLIENT_TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) $(SERVER_TARGET) $(CLIENT_TARGET)

.PHONY: all clean
