# Dropbox Server - Phase 1

A multi-threaded file storage server implementation supporting user authentication and basic file operations.

## Project Structure

dropbox_server/
├── server.c # Main server implementation
├── client.c # Client implementation
├── queue.h/.c # Thread-safe queue implementation
├── threadpool.h/.c # Thread pool management
├── user_auth.h/.c # User authentication and management
├── file_ops.h/.c # File operations (upload/download/delete/list)
├── Makefile # Build configuration
├── test_phase1.sh # Automated test script
└── user_files/ # Server storage directory (auto-created)

## Build Instructions

### Prerequisites
- GCC compiler
- POSIX threads (pthreads)
- Linux environment

### Compilation
```bash
# Build both server and client
make

# Or build manually
gcc -pthread -o dropbox_server server.c queue.c threadpool.c user_auth.c file_ops.c
gcc -o dropbox_client client.c

make clean

./dropbox_server

./dropbox_client

Available Commands

    SIGNUP <username> <password> - Create new user account

    LOGIN <username> <password> - Authenticate user

    UPLOAD <local_file> <server_file> - Upload file to server

    DOWNLOAD <server_file> - Download file from server

    DELETE <server_file> - Delete file from server

    LIST - List user's files and quota usage

    QUIT - Close connection

Automated Test

  ./test_phase1.sh

Manual Test Sequence

    Start server: ./dropbox_server

    Run client: ./dropbox_client

    Execute commands:

SIGNUP user1 pass123
LOGIN user1 pass123
UPLOAD test.txt test.txt
LIST
DOWNLOAD test.txt
DELETE test.txt
LIST
QUIT

Memory Leak Check

valgrind --leak-check=full ./dropbox_server

Architecture

    Main Thread: Accepts incoming connections

    Client Thread Pool: Handles authentication and command parsing

    Worker Thread Pool: Executes file operations

    Thread-safe Queues: Client queue and task queue

    Per-user Storage: Isolated file storage with quota management

File Storage

User files are stored in user_files/<username>/ directory. Each user gets:

    10MB default storage quota

    Individual file tracking

    Thread-safe access control

Limitations (Phase 1)

    Single session per user supported

    Basic error handling

    In-memory user metadata (not persistent)

    No file conflict resolution for multiple clients
