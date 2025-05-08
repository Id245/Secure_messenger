SecureMessenger

A secure messaging application with client-server architecture using encryption for private communications.

## Features

- End-to-end encryption using SSL/TLS
- User registration with unique usernames
- Real-time chat between online users
- Colorful interactive CLI interface
- Message history tracking
- Notifications for new messages
- Multi-user support

## Prerequisites

- C++17 compatible compiler
- CMake (version 3.10 or higher)
- Boost library (with system component)
- OpenSSL library

## Building the Application

1. Clone this repository
2. Make sure you have the prerequisites installed
3. Build using the provided Makefile:

```bash
make build
```

This will:
- Create a build directory
- Configure CMake
- Compile both the server and client applications

## SSL Certificate Generation

Before running the server, you need to generate SSL certificates:

```bash
make generate_certs
```

This command creates:
- `server.key` - Private key for the server
- `server.crt` - Self-signed certificate for SSL encryption

## Running the Application

### Starting the Server

```bash
make run_server
```

Or manually:

```bash
./build/server [port]
```

The default port is 8443 if not specified.

### Starting the Client

```bash
make run_client  # Connects to localhost:8443
```

Or specify a custom server:

```bash
make client ip=192.168.1.100 port=8443
```

Or manually:

```bash
./build/client <server_ip> <port>
```

## Using the Application

1. **Start the server** first
2. **Launch the client** and enter your username
3. **Select a user** to chat with from the list of online users
4. **Send messages** by typing and pressing Enter
5. Type `/back` to return to the user selection screen

## Implementation Details

- Uses Boost.Asio for asynchronous networking
- OpenSSL for secure communication
- JSON for message serialization
- Multi-threaded design for responsive UI

## Commands in Chat

- `/back` - Return to user selection
- Press Enter on an empty message to refresh the chat

## Clean Up

Stop the server and clean the build files:

```bash
make stop_server
make clean
