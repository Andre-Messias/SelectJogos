COMPILER = g++
FLAGS = -Wall -Wextra -std=c++17 -pthread -I$(LIB)

TARGET = game
PORT = 8080

LIB = lib
OBJ_DIR = objs

LIB_HEADERS = $(wildcard $(LIB)/*.hpp)
LIB_SRCS = $(wildcard $(LIB)/*.cpp)

HEADERS = $(wildcard *.hpp)

SRCS = $(wildcard *.cpp) $(LIB_SRCS)
OBJS = $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(SRCS))

all: $(TARGET)

run: all
	./$(TARGET) $(PORT)

$(TARGET): $(OBJS)
	$(COMPILER) $(FLAGS) -o $@ $^

$(OBJ_DIR)/%.o: %.cpp $(HEADERS) $(LIB_HEADERS)
	@mkdir -p $(dir $@)
	$(COMPILER) $(FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all run clean