#include "server.h"
#include <assert.h>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

void buf_append(Buffer &buf, const uint8_t *data, size_t len) {
  buf.insert(buf.end(), data, data + len);
}

void buf_append_u8(Buffer &buf, uint8_t data) { buf.push_back(data); }

void buf_append_u32(Buffer &buf, uint32_t data) {
  buf_append(buf, (const uint8_t *)&data, 4);
}

void buf_append_i64(Buffer &buf, int64_t val) {
  buf_append(buf, (const uint8_t *)&val, 8);
}

static void buf_consume(std::vector<uint8_t> &buf, size_t len) {
  buf.erase(buf.begin(), buf.begin() + len);
}

static void fd_set_nb(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

void die(const char *err_msg) {
  perror(err_msg);
  exit(EXIT_FAILURE);
}

void msg(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
}

int32_t read_full(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = read(fd, buf, n);
    if (rv < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (rv == 0) {
      return -1;
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}

int32_t write_full(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t wv = write(fd, buf, n);
    if (wv <= 0) {
      return -1;
    }
    assert((size_t)wv <= n);
    n -= (size_t)wv;
    buf += wv;
  }
  return 0;
}

Conn *handle_accept(int fd) {
  Conn *conn = new Conn();
  struct sockaddr_in client_addr = {};
  socklen_t socket_len = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &socket_len);
  if (connfd < 0) {
    return NULL;
  }
  fd_set_nb(connfd);
  conn->fd = connfd;
  conn->want_read = true;
  return conn;
}

void handle_read(Conn *conn) {
  uint8_t read_buf[64 * 1024];
  ssize_t rv = read(conn->fd, read_buf, sizeof(read_buf));
  if (rv <= 0) {
    conn->want_close = true;
    return;
  }
  buf_append(conn->incoming, read_buf, (size_t)rv);
  while (try_one_request(conn)) {
  }
  if (conn->outgoing.size() > 0) {
    conn->want_read = false;
    conn->want_write = true;
    return handle_write(conn);
  }
}

static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out) {
  if (cur + 4 > end) {
    return false;
  }
  memcpy(&out, cur, 4);
  cur += 4;
  return true;
}

static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out) {
  if (cur + n > end) {
    return false;
  }
  out.assign(cur, cur + n);
  cur += n;
  return true;
}

static int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out) {
  const uint8_t *end = data + size;
  uint32_t nstr = 0;
  if (!read_u32(data, end, nstr)) {
    return -1;
  }
  if (nstr > k_max_args) {
    return -1;
  }
  while (out.size() < nstr) {
    uint32_t len = 0;
    if (!read_u32(data, end, len)) {
      return -1;
    }
    out.push_back(std::string());
    if (!read_str(data, end, len, out.back())) {
      return -1;
    }
  }
  if (data != end) {
    return -1;
  }
  return 0;
}

void out_nil(Buffer &out) { buf_append_u8(out, TAG_NIL); }

void out_str(Buffer &out, const char *str, size_t len) {
  buf_append_u8(out, TAG_STR);
  buf_append_u32(out, (uint32_t)len);
  buf_append(out, (const uint8_t *)str, len);
}

void out_int(Buffer &out, int64_t val) {
  buf_append_u8(out, TAG_INT);
  buf_append_i64(out, val);
}

void out_arr(Buffer &out, uint32_t n) {
  buf_append_u8(out, TAG_ARR);
  buf_append_u32(out, n);
}

void out_err(Buffer &out, uint32_t code, const char *msg, size_t len) {
  buf_append_u8(out, TAG_ERR);
  buf_append_u32(out, code);
  buf_append_u32(out, (uint32_t)len);
  buf_append(out, (const uint8_t *)msg, len);
}

void response_begin(Buffer &out, size_t *header) {
  *header = out.size();
  buf_append_u32(out, 0);
}

static size_t response_size(Buffer &out, size_t header) { return out.size() - header - 4; }

void response_end(Buffer &out, size_t header) {
  size_t msg_size = response_size(out, header);
  if (msg_size > k_max_msg) {
    out.resize(header + 4);
    const char *err_msg = "response size is too big";
    out_err(out, ERR_TOO_BIG, err_msg, strlen(err_msg));
    msg_size = response_size(out, header);
  }
  uint32_t len = (uint32_t)msg_size;
  memcpy(&out[header], &len, 4);
}

bool try_one_request(Conn *conn) {
  if (conn->incoming.size() < 4) {
    return false;
  }
  uint32_t len = 0;
  std::memcpy(&len, conn->incoming.data(), 4);
  if (len > k_max_msg) {
    conn->want_close = true;
    return false;
  }
  if (len + 4 > conn->incoming.size()) {
    return false;
  }

  const uint8_t *request = &conn->incoming[4];
  std::vector<std::string> cmd;
  if (parse_req(request, len, cmd) < 0) {
    conn->want_close = true;
    return false;
  }

  size_t header_pos = 0;
  response_begin(conn->outgoing, &header_pos);
  do_request(cmd, conn->outgoing);
  response_end(conn->outgoing, header_pos);

  buf_consume(conn->incoming, 4 + len);
  return true;
}

void handle_write(Conn *conn) {
  assert(conn->outgoing.size() > 0);
  ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
  if (rv < 0 && errno == EAGAIN) {
    return;
  }
  if (rv < 0) {
    conn->want_close = true;
    return;
  }
  buf_consume(conn->outgoing, rv);
  if (conn->outgoing.size() == 0) {
    conn->want_read = true;
    conn->want_write = false;
  }
}