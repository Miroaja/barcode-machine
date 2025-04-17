# Compiler
CXX = clang++

# Compiler flags
CXXFLAGS = -O3 -std=c++23 -Wall -Wextra -Werror

# Source files
SERVER_SRCS = server_main.cpp  # Add your server source files here
CLIENT_SRCS = client_main.cpp  # Add your client source files here

# Object files
SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.cpp=.o)

# Output executables
SERVER_TARGET = server_program  # Change this to your desired server output name
CLIENT_TARGET = client_program  # Change this to your desired client output name

# Default target
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Link the server object files to create the server executable
$(SERVER_TARGET): $(SERVER_OBJS)
	$(CXX) $(SERVER_OBJS) -o $(SERVER_TARGET)

# Link the client object files to create the client executable
$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CXX) $(CLIENT_OBJS) -o $(CLIENT_TARGET)

# Compile server source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) $(SERVER_TARGET) $(CLIENT_TARGET)

.PHONY: all clean
