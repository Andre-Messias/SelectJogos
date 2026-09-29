PORT ?= 8080
HOST ?= 127.0.0.1
CONFIG ?= game.config
HELP ?= help.txt

SRC_DIR = src
TEMPLATE_GAME_DIR = games/template

all: games src

# Compiles all modules inside src/ (Lobby Server + TUI Client)
src:
	$(MAKE) -C $(SRC_DIR) all

# Compiles the game template (and can be extended to other games)
games:
	$(MAKE) -C $(TEMPLATE_GAME_DIR) all

server:
	$(MAKE) -C $(SRC_DIR) server

client:
	$(MAKE) -C $(SRC_DIR) client

# Compiles games + server + client, starts Lobby in background, and opens TUI Client
run: all
	$(MAKE) -C $(SRC_DIR) run HOST=$(HOST) PORT=$(PORT) CONFIG=$(CONFIG) HELP=$(HELP)

run-server: games server
	$(MAKE) -C $(SRC_DIR) run-server PORT=$(PORT) CONFIG=$(CONFIG)

run-client: client
	$(MAKE) -C $(SRC_DIR) run-client HOST=$(HOST) PORT=$(PORT) HELP=$(HELP)

clean:
	$(MAKE) -C $(SRC_DIR) clean
	$(MAKE) -C $(TEMPLATE_GAME_DIR) clean

.PHONY: all src games server client run run-server run-client clean