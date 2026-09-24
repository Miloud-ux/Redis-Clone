# Redis Clone

A Redis server and client built from scratch.

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Build](#build)
- [Usage](#usage)
- [Protocol & Features](#protocol--features)
- [Resources](#resources)

## Overview

- Single-threaded TCP server using an event loop built on `poll()`.
- Non-blocking IO with buffered reads and writes.
- Handles pipelined requests (multiple messages per read).
- **Custom Hashtable**: In-memory key-value store with incremental rehashing and collision handling.
- **KV Commands**: Supports basic `GET`, `SET`, and `DEL` operations.
- **TLV Serialization**: Client-server communication uses Type-Length-Value encoding.
- Client for testing the server, including a 32 MB message stress test.

## Project Structure

- `server.h`: shared declarations, `Conn` struct, common helpers.
- `common.cpp`: shared implementations used by server and client.
- `server.cpp`: event loop, socket setup, connection handling.
- `client.cpp`: test client that speaks the request-response protocol and formats KV commands.
- `hashtable.h` & `hashtable.cpp`: Implementation of the custom hash table and dictionary mechanics.
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

The server listens on port 1234. It reads requests in an event loop and sends responses back in order.

## Protocol & Features

- **Serialization**: Commands are serialized using Type-Length-Value (TLV) encoding.
- **Message Framing**: Each message starts with a 4-byte length prefix (little endian) followed by the payload.
- **Pipelining**: Multiple messages may arrive in a single read; the server buffers and parses them as a byte stream.

## Resources

- **[Build Your Own Redis in C/C++]**: This was the main resource I relied on to build this project.