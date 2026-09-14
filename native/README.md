# Helion native server and OpenGL client

This standalone C++17 pair uses POSIX TCP sockets. The client opens an SDL2
window with an OpenGL 2.1-compatible fixed-function renderer (no shaders,
VAOs, or modern OpenGL requirements). Networking and the command parser remain
usable from a terminal with `--terminal`.

## Build

From the repository root:

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel
```

The build finds SDL2 and OpenGL through CMake (`SDL2::SDL2`, `OpenGL::GL`).

## Run

In one terminal, `./build-native/native/helion_server 4242 helion-server.db`.
In another, `./build-native/native/helion_client 127.0.0.1 4242`.
This opens the window; type protocol commands in the window and press Enter.
For the retained command-line mode, use
`./build-native/native/helion_client 127.0.0.1 4242 --terminal`.

The client commands are `/create username password display`, `/login username
password`, `/chat message`, `/profile`, `/state`, `/help`, and `/quit`. The optional server data-file stores commander profiles and GalNet
messages durably and is replaced atomically after each write.

## Line protocol

The server sends `WELCOME` and `INFO` lines on connect. Requests are one
newline-terminated line: `CREATE username password display`, `LOGIN username
password`, `CHAT message`, `STATE`, or `QUIT`. Responses are newline-terminated
`OK`, `ERR`, `PROFILE`, `STATE`, and `CHAT` records. Passwords are stored as local server
credentials in the data file; use filesystem permissions to protect it.
