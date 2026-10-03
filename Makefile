PORT ?= 8080
HOST ?= 127.0.0.1
CONFIG ?= game.config
HELP ?= help.txt

SRC_DIR = src

# List all active games here
GAMES_LIST = template minesweeper

all: games src

# Compiles all modules inside src/ (Lobby Server + TUI Client)
src:
	$(MAKE) -C $(SRC_DIR) all

# Compiles all games in the GAMES_LIST
games:
	@for game in $(GAMES_LIST); do \
		$(MAKE) -C games/$$game all; \
	done

server:
	$(MAKE) -C $(SRC_DIR) server

client:
	$(MAKE) -C $(SRC_DIR) client

# Compiles games + server + client, starts Lobby in background, and opens TUI Client
run: all
	$(MAKE) -C $(SRC_DIR) run HOST=$(HOST) PORT=$(PORT) CONFIG=$(CONFIG) HELP=$(HELP)

run-server: games server
	$(MAKE) -C $(SRC_DIR) run-server PORT=$(PORT) CONFIG=$(CONFIG)

stop-server:
	$(MAKE) -C $(SRC_DIR) stop-server PORT=$(PORT)

run-client: client
	$(MAKE) -C $(SRC_DIR) run-client HOST=$(HOST) PORT=$(PORT) HELP=$(HELP)

clean:
	$(MAKE) -C $(SRC_DIR) clean
	@for game in $(GAMES_LIST); do \
		$(MAKE) -C games/$$game clean; \
	done

.PHONY: all src games server client run run-server stop-server run-client clean