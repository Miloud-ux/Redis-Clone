# Redis Clone

A Redis server and client built from scratch following the book "Build Your Own Redis with C/C++".

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Build](#build)
- [Usage](#usage)
- [Protocol](#protocol)

## Overview

- Single-threaded TCP server using an event loop built on `poll()`.
- Non-blocking IO with buffered reads and writes.
- Simple client for testing the server.

## Project Structure

- `server.h`: shared declarations, `Conn` struct, common helpers.
- `common.cpp`: implementations of the shared helpers and application callbacks.
- `server.cpp`: event loop, socket setup, connection handling.
- `client.cpp`: test client that speaks the request-response protocol.
- `Makefile`: builds the server and client targets.

## Build

```sh
make
```

This produces the `server` and `client` binaries.

## Usage

Start the server in one terminal:

```sh
./server
```

Run the client in another terminal:

```sh
./client
```

The server listens on port 1234 and echoes back the messages it receives.

## Protocol

Each message is a 4-byte length prefix (little endian) followed by that many bytes of payload.