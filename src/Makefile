PORT ?= 8080
HOST ?= 127.0.0.1
CONFIG ?= game.config
HELP ?= help.txt

SERVER_DIR = server
CLIENT_DIR = client

all: server client

server:
	$(MAKE) -C $(SERVER_DIR) all

client:
	$(MAKE) -C $(CLIENT_DIR) all

# Starts the Lobby server in the background (logging to server/lobby.log)
# and launches the interactive TUI Client in the foreground.
run: all
	@echo "Starting Lobby Server on port $(PORT)..."
	@cd $(SERVER_DIR) && ./lobby $(PORT) $(CONFIG) > lobby.log 2>&1 & \
	SERVER_PID=$$!; \
	sleep 0.3; \
	$(MAKE) -s -C $(CLIENT_DIR) run HOST=$(HOST) PORT=$(PORT) HELP=$(HELP); \
	kill $$SERVER_PID 2>/dev/null || true

run-server:
	$(MAKE) -C $(SERVER_DIR) run PORT=$(PORT) CONFIG=$(CONFIG)

run-client:
	$(MAKE) -C $(CLIENT_DIR) run HOST=$(HOST) PORT=$(PORT) HELP=$(HELP)

clean:
	$(MAKE) -C $(SERVER_DIR) clean
	$(MAKE) -C $(CLIENT_DIR) clean
	rm -f $(SERVER_DIR)/lobby.log

.PHONY: all server client run run-server run-client clean