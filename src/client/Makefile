COMPILER = g++
FLAGS = -Wall -Wextra -std=c++17 -pthread -I$(INC_DIR) -I$(SRC_DIR)

TARGET = client
HOST ?= 127.0.0.1
PORT ?= 8080

INC_DIR = include
SRC_DIR = src
OBJ_DIR = objs

HEADERS = $(wildcard $(INC_DIR)/*.hpp $(SRC_DIR)/*.hpp $(SRC_DIR)/*/*.hpp)
SRCS = $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/*/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

all: $(TARGET)

run: all
	./$(TARGET) $(HOST) $(PORT)

$(TARGET): $(OBJS)
	$(COMPILER) $(FLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(COMPILER) $(FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all run clean