COMPILER = g++
FLAGS = -Wall -Wextra -std=c++17 -pthread -Isrc -Isrc/lib
SRCS = $(wildcard src/*.cpp src/lib/*.cpp)
OBJS = $(patsubst src/%.cpp,objs/%.o,$(SRCS))

all: build/game

build/game: $(OBJS)
	@mkdir -p build
	$(COMPILER) $(FLAGS) -o $@ $^

objs/%.o: src/%.cpp $(wildcard src/*.hpp src/lib/*.hpp)
	@mkdir -p $(dir $@)
	$(COMPILER) $(FLAGS) -c $< -o $@

clean:
	rm -rf objs build

.PHONY: all clean
