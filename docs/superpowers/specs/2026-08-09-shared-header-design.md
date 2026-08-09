# Design: Shared `server.h` + `common.cpp` + Makefile

## Context

`server.cpp` and `client.cpp` each define the same helpers: `k_max_msg`,
`die`, `msg`, `read_full`, `write_full`. Duplicated code must be de-duplicated
into one shared module. A simple Makefile is needed to build the two binaries
against the new shared translation unit.

## Design

### `server.h` — declarations only

```c
#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr size_t k_max_msg = 4096;

void die(const char *err_msg);
void msg(const char *fmt, ...);
int32_t read_full(int fd, char *buf, size_t n);
int32_t write_full(int fd, char *buf, size_t n);
```

### `common.cpp` — single copy of definitions

Contains the implementations of `die`, `msg`, `read_full`, `write_full`,
plus the include of `server.h` and the required system headers.

### `server.cpp` / `client.cpp`

- Remove the duplicated helper blocks.
- `#include "server.h"`.
- `one_request`, `query`, and `main` stay unchanged.

### Makefile

Two targets: `server` (from `server.cpp common.cpp`) and `client` (from
`client.cpp common.cpp`). Standard variables (`CXX`, `CXXFLAGS`), `all` as
the default target, and `clean`.

## Key decisions

1. `k_max_msg` is `constexpr` in the header, not `extern const` in the .cpp.
   Both files size stack arrays with it (`char rbuf[4 + k_max_msg]`), which
   requires a compile-time constant in C++.
2. `read_full` uses the client's version (with `EINTR` retry). It is strictly
   more robust than the server's version, and both files must share one
   definition.
3. Helpers lose `static` — they need external linkage to be shared across
   translation units.
4. Makefile over CMake: only two small source files, no build complexity.

## Build

```sh
make          # builds ./server and ./client
make clean    # removes binaries
```
