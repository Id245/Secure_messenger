# Variables
BUILD_DIR = build
SERVER_BIN = $(BUILD_DIR)/server
CLIENT_BIN = $(BUILD_DIR)/client
SERVER_PORT = 8443
SERVER_IP = 127.0.0.1
CERT_DIR = certs

# Default rule
all: build

# Build the project
build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DBOOST_ROOT=/opt/homebrew -DBOOST_INCLUDEDIR=/opt/homebrew/include -DBOOST_LIBRARYDIR=/opt/homebrew/lib && make -j4
	@echo "Build completed successfully!"

# Generate SSL certificates
generate_certs:
	@echo "Generating SSL certificates..."
	@openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes -subj '/CN=localhost'
	@echo "Certificates generated successfully!"

# Stop any running server processes
stop_server:
	@echo "Stopping any running server processes..."
	@-pkill -f $(SERVER_BIN) || echo "No server processes found"
	@-lsof -i :$(SERVER_PORT) -t | xargs kill -9 2>/dev/null || echo "Port $(SERVER_PORT) is free"
	@sleep 1

# Run the server in the background
run_server: build generate_certs stop_server
	@echo "Starting secure server..."
	@$(SERVER_BIN) $(SERVER_PORT) &
	@echo "Server started with PID: $$!"
	@sleep 1

# Run the client with default localhost settings
run_client: build
	@echo "Starting client with default settings (localhost:$(SERVER_PORT))..."
	@$(CLIENT_BIN) $(SERVER_IP) $(SERVER_PORT)

# Run client with custom parameters
client: build
	@if [ -z "$(ip)" ] || [ -z "$(port)" ]; then \
		echo "Usage: make client ip=<server_ip> port=<server_port>"; \
	else \
		echo "Starting client with connection to $(ip):$(port)..."; \
		$(CLIENT_BIN) $(ip) $(port); \
	fi

# Run both server and client (for testing on local machine)
run: run_server run_client

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
	@rm -f server.crt server.key
	@echo "Clean completed!"

# Clean and rebuild
rebuild: clean build

# Run unit tests
test: build
	@echo "Running unit tests..."
	@$(BUILD_DIR)/unit_tests

.PHONY: all build generate_certs stop_server run_server run_client client run clean rebuild test
