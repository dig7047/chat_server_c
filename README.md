# Dual-Stack Chat Server in C

A single-threaded client-server chat application written in C. 
This project implements an event-driven architecture to handle concurrent connections efficiently without the overhead of multithreading, ensuring lock-free state management and seamless support for both IPv4 and IPv6 networks.

## Key Technical Features

* **Event-Driven Multiplexing:** Both the server and the client utilize `select()` for non-blocking I/O operations. The server handles multiple clients using a single-thread state machine, completely avoiding race conditions, deadlocks, and the need for mutexes.
* **Native Dual-Stack (IPv4/IPv6):** The server listens on an `AF_INET6` socket with `IPV6_V6ONLY` disabled, transparently accepting both IPv4 (via IPv4-mapped IPv6 addresses) and pure IPv6 clients. The client dynamically allocates memory using `struct sockaddr_storage` to adapt to the user's IP input.
* **TCP & UDP Channel Separation:**
  * **TCP** is used for the primary reliable command/data stream.
  * **UDP** is utilized by the client as a passive background listener for asynchronous server notifications, avoiding interruptions to the active STDIN input flow.
* **Robust Signal Handling:** Implements graceful shutdown routines intercepting `SIGINT` and `SIGTERM` to properly close file descriptors and free ports, while explicitly ignoring `SIGPIPE` to prevent sudden server crashes on abrupt client disconnections.
* **POSIX Compliant:** Built using standard POSIX APIs (e.g., standard `FD_SETSIZE` instead of compiler-specific macros), ensuring cross-platform compatibility across Linux and macOS.

## Architecture Overview

### The Server
The core of the server acts as a central dispatcher (`server_run`). It maintains an active list of file descriptors (`master_fds`) and uses `select()` to wake up only when a network event occurs. Connections are identified by their file descriptors and mapped to an authenticated `ClientStore` structure only after successful authentication (`REGIS` or `CONNE`).

### The Client
The client relies on a 3-way multiplexed loop listening simultaneously to:
1. `STDIN_FILENO` (User keyboard input)
2. `tcp_fd` (Active connection to the server)
3. `udp_fd` (Passive listening socket for real-time notifications)

## Build Instructions

This project includes a `Makefile` for easy compilation. It requires `gcc` (or `clang` on macOS) and standard C libraries.

To build both the server and the client, simply run:

```bash
make
```

*(This will generate the `server_app` and `client_app` executables).*

To clean up the directory by removing the compiled object files and executables, run:

```bash
make clean
```

## Usage

### Starting the Server
Start the server by specifying the listening port. You can use the optional `-v` flag to enable verbose logging.

```bash
./server_app <port> [-v]

# Example:
./server_app 8000 -v
```

### Starting the Client
Start the client by providing the server's IP address (IPv4 or IPv6), the server's TCP port, and a local UDP port for receiving notifications.

```bash
./client_app <server_ip> <server_tcp_port> <local_udp_port> [-v]

# Example (IPv4 - Localhost):
./client_app 127.0.0.1 8000 8001 -v

# Example (IPv6 - Localhost):
./client_app ::1 8000 8002 -v

# Example (Connecting to a remote server in LAN):
./client_app 192.168.1.50 8000 8003
```

## Custom Protocol Commands

The application uses a fixed-length 5-byte header protocol for parsing commands. Supported operations include:

* `REGIS`: Register a new user.
* `CONNE`: Authenticate an existing user.
* `FRIE?`: Send friend request.
* `MESS?`: Send messages.
* `FLOO?`: Broadcast flood messages across the friend graph.
* `LIST?`: List users.
* `IQUIT`: Graceful disconnection.
