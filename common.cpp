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
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
  buf.insert(buf.end(), data, data + len);
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
        continue; // interrupted by a sig before reading (retry)
      }
      return -1; // error
    }

    if (rv == 0) {
      return -1; // Unexpected EOF (connection closed before reading anything)
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
  if (!conn) {
    return NULL;
  }

  struct sockaddr_in client_addr = {};
  socklen_t socket_len = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &socket_len);
  if (connfd < 0) {
    return NULL;
  }
  fd_set_nb(connfd);
  conn->fd = connfd;
  conn->want_read = true; // read the first request
  return conn;
}

void handle_read(Conn *conn) {
  // 1. Do a non-blocking read
  // 2. Accumilate data
  // 3. Parse message if it's enough else do nothing
  // 4. Process the parsed message
  // 5. remove msg from Conn::incoming

  uint8_t read_buf[64 * 1024]; // read buffer 64kb
  ssize_t rv = read(conn->fd, read_buf, sizeof(read_buf));

  if (rv <= 0) { // Handle I/O error (rv < 0) or EOF(rv == 0)
    conn->want_close = true;
    return;
  }

  buf_append(conn->incoming, read_buf, (size_t)rv);
  /*  == Batch multiple requests ==
   * this tiny change plays a huge role */
  while (try_one_request(conn)) {
  }

  if (conn->outgoing.size() > 0) {
    conn->want_read = false;
    conn->want_write = true;
    return handle_write(conn);
  }
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

  // generate the response (echo it back)
  uint8_t *request = &conn->incoming[4];
  buf_append(conn->outgoing, (const uint8_t *)&len, 4);
  buf_append(conn->outgoing, request, len);

  // consume the message from incoming
  buf_consume(conn->incoming, len + 4);

  /*  in request-response protocols you can either
   *  write a request or read a response that's why
   *  we change the state. Note that this is not always
   *  the case and some protocols can read and write simultanously
   */

  return true;
}

void handle_write(Conn *conn) {
  assert(conn->outgoing.size() > 0);
  ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());

  if (rv < 0 && errno == EAGAIN) {
    return; // full buffer
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
