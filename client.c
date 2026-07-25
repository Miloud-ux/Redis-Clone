#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

const size_t k_max_msg = 4096;

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

static int32_t read_full(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = read(fd, buf, n);
    if (rv <= 0) {
      return -1; // error or unexpected EOF
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }
  return 0;
}

static int32_t write_full(int fd, char *buf, size_t n) {
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

int32_t query(int fd, const char *text) {
  uint32_t len = (uint32_t)strlen(text);
  if (len > k_max_msg) {
    return -1;
  }

  char wbuf[k_max_msg + 4];
  memcpy(wbuf, &len, 4);
  memcpy(&wbuf[4], text, len);

  int32_t err = write_full(fd, wbuf, 4 + len);
  if (err) {
    return err;
  }

  char rbuf[4 + k_max_msg + 1];
  errno = 0;
  err = read_full(fd, rbuf, 4);
  if (err) {
    msg(errno == 0 ? "EOF" : "Read() error");
    return err;
  }

  memcpy(&len, rbuf, 4);
  if (len > k_max_msg) {
    msg("msg too long");
    return -1;
  }

  err = read_full(fd, &rbuf[4], len);
  if (err) {
    msg("read() error");
    return err;
  }

  printf("server says: %.*s\n", len, &rbuf[4]);
  return 0;
}

int main() {

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }

  struct sockaddr_in serv_addr = {.sin_family = AF_INET,
                                  .sin_port = htons(1234),
                                  .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};

  int rv = connect(fd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));

  if (rv) {
    die("connect()");
  }

  int32_t err = query(fd, "hello1");
  if (err) {
    goto L_DONE;
  }

  err = query(fd, "hello2");
  if (err) {
    goto L_DONE;
  }

L_DONE:
  close(fd);
  return 0;
}
