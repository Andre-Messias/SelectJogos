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
	@bash -c '\
		cd $(SERVER_DIR) && ./lobby $(PORT) $(CONFIG) > lobby.log 2>&1 & \
		SERVER_PID=$$!; \
		trap "kill $$SERVER_PID 2>/dev/null || true" EXIT INT TERM; \
		sleep 0.3; \
		$(MAKE) -s -C $(CLIENT_DIR) run HOST=$(HOST) PORT=$(PORT) HELP=$(HELP) \
	'

run-server:
ifdef DAEMON
	@echo "Starting Lobby Server in daemon mode (background)..."
	@nohup $(MAKE) -C $(SERVER_DIR) run PORT=$(PORT) CONFIG=$(CONFIG) > server_daemon.log 2>&1 & echo "Server running on PID $$!"
else
	$(MAKE) -C $(SERVER_DIR) run PORT=$(PORT) CONFIG=$(CONFIG)
endif

stop-server:
	@echo "Stopping Lobby Server..."
	@pkill -f "./lobby $(PORT)" && echo "Server stopped." || echo "Server is not running."

run-client:
	$(MAKE) -C $(CLIENT_DIR) run HOST=$(HOST) PORT=$(PORT) HELP=$(HELP)

clean:
	$(MAKE) -C $(SERVER_DIR) clean
	$(MAKE) -C $(CLIENT_DIR) clean
	rm -f $(SERVER_DIR)/lobby.log

.PHONY: all server client run run-server stop-server run-client clean