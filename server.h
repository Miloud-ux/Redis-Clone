#pragma once
#include <cstddef>
#include <cstdint>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

constexpr size_t k_max_msg = 32 << 20;
constexpr size_t k_max_args = 1024;

typedef std::vector<uint8_t> Buffer;

enum {
  TAG_NIL = 0,
  TAG_ERR = 1,
  TAG_STR = 2,
  TAG_INT = 3,
  TAG_DBL = 4,
  TAG_ARR = 5,
};

enum {
  ERR_UNKNOWN = 1,
  ERR_TOO_BIG = 2,
};

struct Conn {
  int fd = -1;
  bool want_write = false;
  bool want_close = false;
  bool want_read = false;

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

void buf_append(Buffer &buf, const uint8_t *data, size_t len);
void buf_append_u8(Buffer &buf, uint8_t data);
void buf_append_u32(Buffer &buf, uint32_t data);
void buf_append_i64(Buffer &buf, int64_t val);
void out_nil(Buffer &out);
void out_str(Buffer &out, const char *str, size_t len);
void out_int(Buffer &out, int64_t val);
void out_arr(Buffer &out, uint32_t n);
void out_err(Buffer &out, uint32_t code, const char *msg, size_t len);
void response_begin(Buffer &out, size_t *header);
void response_end(Buffer &out, size_t header);

uint64_t str_hash(const uint8_t *data, size_t len);
void do_request(std::vector<std::string> &cmd, Buffer &out);