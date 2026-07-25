#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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

static int32_t one_request(int connfd) {
  // 4bytes header
  char rbuf[4 + k_max_msg];
  errno = 0;
  int32_t err = read_full(connfd, rbuf, 4);

  if (err) {
    msg(errno == 0 ? "EOF" : "read() error");
    return -1;
  }

  uint32_t len = 0;
  memcpy(&len, rbuf, 4);
  if (len > k_max_msg) {
    msg("msg too long");
    return -1;
  }

  // request body
  err = read_full(connfd, &rbuf[4], len);
  if (err) {
    msg("read() error");
    return -1;
  }

  // do something
  printf("client says: %.*s\n", len, &rbuf[4]);
  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)];
  len = (uint32_t)strlen(reply);
  memcpy(&wbuf, &len, 4);
  memcpy(&wbuf[4], reply, len);

  return write_full(connfd, wbuf, len + 4);
}

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0); // IPV4 TCP socket
  if (fd < 0) {
    die("socket()");
  }
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(1234),
                             .sin_addr.s_addr =
                                 htonl(INADDR_ANY)}; // to fix endianess
  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));

  if (rv) {
    die("bind()");
  }

  // 40% (size of the queue) doesn't matter on linux anyway.
  rv = listen(fd, SOMAXCONN);

  while (1) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);

    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
      continue;
    }

    printf("Client address: %s\n", inet_ntoa(client_addr.sin_addr));

    while (1) {
      int32_t err = one_request(connfd);
      if (err) {
        break;
      }
    }

    close(connfd);
  }

  return 0;
}
