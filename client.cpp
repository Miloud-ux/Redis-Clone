#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "server.h"

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

  struct sockaddr_in serv_addr = {};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(1234);
  serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

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
