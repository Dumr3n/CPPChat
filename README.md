# CPPChat

A terminal-based, multi-client TCP chat for Windows, written in modern C++.

## Features

- Length-prefixed binary protocol that handles TCP fragmentation and coalescing
- RAII-managed Winsock sessions and sockets
- Thread-safe broadcasts and graceful Ctrl+C shutdown
- Username validation, duplicate-name detection, timestamps, and join/leave messages
- `/help`, `/users`, `/nick <name>`, `/me <action>`, and `/quit` commands
- Configurable bind address, server address, port, and username

## Build

Requires CMake 3.20+, a C++20 compiler, and Windows/Winsock.

```powershell
cmake --preset debug
cmake --build --preset debug
```

For an optimized build, replace `debug` with `release`. CMake files are stored
under `build/debug` or `build/release`.

The first configure downloads the pinned GoogleTest dependency. Run the tests with:

```powershell
ctest --preset debug
```

## Run

Start the server:

```powershell
.\build\debug\Debug\server.exe
```

Then start two or more clients:

```powershell
.\build\debug\Debug\client.exe --name alice
.\build\debug\Debug\client.exe --name bob
```

Use `--help` to see command-line options. The default endpoint is
`127.0.0.1:54000`. To allow LAN connections, explicitly bind the server to an
appropriate local interface and permit the chosen port through Windows Firewall.

## Protocol

Each packet contains a one-byte message type, a four-byte payload size in network
byte order, and a UTF-8 payload. Payloads are limited to 4096 bytes. This project
does not currently provide encryption or authentication; do not expose it directly
to the public internet.
