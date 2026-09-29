COMPILER = g++
FLAGS = -Wall -Wextra -std=c++17 -pthread -I$(SRC_DIR) -I$(LIB_DIR)

BUILD_DIR = build
TARGET = $(BUILD_DIR)/game
PORT ?= 8080

SRC_DIR = src
LIB_DIR = $(SRC_DIR)/lib
OBJ_DIR = objs

HEADERS = $(wildcard $(SRC_DIR)/*.hpp $(LIB_DIR)/*.hpp)
SRCS = $(wildcard $(SRC_DIR)/*.cpp $(LIB_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

all: $(TARGET)

run: all
	./$(TARGET) $(PORT)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(COMPILER) $(FLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(COMPILER) $(FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BUILD_DIR)

.PHONY: all run clean