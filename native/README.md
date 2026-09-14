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

`HELION_BUILD_SERVER`, `HELION_BUILD_CLIENT`, and `HELION_BUILD_TESTS` are
independent CMake options and default to `ON`. For a headless server without
SDL2 or OpenGL, configure with `-DHELION_BUILD_CLIENT=OFF`. To build only the
client, use `-DHELION_BUILD_SERVER=OFF`. Client builds require SDL2 and OpenGL;
server and protocol-test builds do not. Run `ctest --test-dir build-native
--output-on-failure` after building.

## Run

In one terminal, `./build-native/native/helion_server 4242 helion-server.db`.
In another, `./build-native/native/helion_client 127.0.0.1 4242`.
This opens the window; type protocol commands in the window and press Enter.
For the retained command-line mode, use
`./build-native/native/helion_client 127.0.0.1 4242 --terminal`.

The client commands are `/create username password display`, `/login username
password`, `/chat message`, `/profile`, `/state`, and `/quit`. The optional server data-file stores commander profiles and GalNet
messages durably and is replaced atomically after each write.

## Line protocol

The server sends `WELCOME Helion/1` and `INFO` lines on connect. Protocol version
1 is defined in `shared/protocol.h`; the client rejects a different greeting.
Requests are one
newline-terminated line: `CREATE username password display`, `LOGIN username
password`, `CHAT message`, `STATE`, or `QUIT`. Responses are newline-terminated
`OK`, `ERR`, `PROFILE`, `STATE`, and `CHAT` records. Passwords are stored as local server
credentials in the data file; use filesystem permissions to protect it.

The shared line decoder accepts at most 4096 bytes before LF (including an
optional CR), buffers partial reads, and emits each complete line in order.
Oversized lines are discarded through the next LF and receive one
`ERR line-too-long`; control-byte or malformed requests receive
`ERR malformed-message`. Processing resumes at the next line. The maximum
chat payload is 512 bytes.
