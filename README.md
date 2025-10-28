# OS Lab Project 2: Dropbox Clone

This project implements a multi-threaded file server with per-user accounts, file operations, and fine-grained concurrency control.

## How to Compile

A `Makefile` is provided. Simply run:

make

This will create two executables: dropbox_server and dropbox_client.

How to Run the Server
./dropbox_server

How to Run the Client
In a new terminal, run:
./dropbox_client

How to Test for Correctness

The Makefile includes rules for running the server with Valgrind (for memory leaks) and ThreadSanitizer (for data races).

1. Test for Memory Leaks (Valgrind)

This command cleans, builds, and runs the server inside Valgrind.
make memcheck

After connecting with clients and running commands, shut down the server with Ctrl+C. The output should show "0 errors from 0 contexts" and "All heap blocks were freed".

2. Test for Data Races (ThreadSanitizer)

This command cleans, builds, and runs the server with ThreadSanitizer enabled.
make racecheck

While the server is running, you can perform the concurrency test.

3. Manual Concurrency Test (Instructions)

This test demonstrates the per-file locking mechanism.

    Start the server with make racecheck.

    Open Terminal A and run ./dropbox_client.

    Open Terminal B and run ./dropbox_client.

    In Terminal A: SIGNUP test 123

    In Terminal A: LOGIN test 123

    In Terminal B: LOGIN test 123

    In Terminal A: UPLOAD my_file.txt dummy.txt (This will pause for 5 sec due to the debug sleep).

    In Terminal B (quickly): DELETE my_file.txt

Expected Result: The DELETE command in Terminal B will hang until the UPLOAD in Terminal A completes, proving the per-file lock is working. The server will not crash and not report a "DATA RACE" warning.
