#pragma once
#include <cstdint>
#include <stddef.h>
#include <stdint.h>
#include <vector>

constexpr size_t k_max_msg = 32 << 20; // huge number
struct Conn {
  int fd = -1;
  // application's intention, for the event loop  bool want_read = false;
  bool want_write = false;
  bool want_close = false;
  bool want_read = false;

  // buffered input and output
  std::vector<uint8_t> incoming;
  std::vector<uint8_t> outgoing;
};

void die(const char *err_msg);
void msg(const char *fmt, ...);
int32_t read_full(int fd, char *buf, size_t n);
int32_t write_full(int fd, char *buf, size_t n);
Conn *handle_accept(int fd);
void handle_read(Conn *conn);
bool try_one_request(Conn *conn);
void handle_write(Conn *conn);
