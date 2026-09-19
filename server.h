#pragma once
#include <cstddef>
#include <cstdint>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

typedef std::vector<uint8_t> Buffer;
constexpr size_t k_max_msg = 32 << 20; // veri big number

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

enum {
  TAG_NIL = 0, // nil
  TAG_ERR = 1, // error code + msg
  TAG_STR = 2, // string
  TAG_INT = 3, // int64
  TAG_DBL = 4, // double
  TAG_ARR = 5, // array
};

void die(const char *err_msg);
void msg(const char *fmt, ...);
int32_t read_full(int fd, char *buf, size_t n);
int32_t write_full(int fd, char *buf, size_t n);
Conn *handle_accept(int fd);
void handle_read(Conn *conn);
bool try_one_request(Conn *conn);
void handle_write(Conn *conn);
void do_request(std::vector<std::string> &cmd, Buffer &outgoing);
void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len);
