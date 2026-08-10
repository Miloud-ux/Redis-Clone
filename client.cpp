#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "server.h"

int32_t query(int fd, const char *text) {
  uint32_t len = (uint32_t)strlen(text);
  if (len > k_max_msg) {
    return -1;
  }

  std::vector<uint8_t> wbuf(4 + len);
  memcpy(wbuf.data(), &len, 4);
  memcpy(&wbuf[4], text, len);

  int32_t err = write_full(fd, (char *)wbuf.data(), 4 + len);
  if (err) {
    return err;
  }
  return 0;
}

int32_t read_res(int fd, size_t len) {

  std::vector<uint8_t> rbuf(4 + k_max_msg + 1);
  errno = 0;
  int32_t err = read_full(fd, (char *)rbuf.data(), 4);
  if (err) {
    msg(errno == 0 ? "EOF" : "Read() error");
    return err;
  }

  memcpy(&len, rbuf.data(), 4);
  if (len > k_max_msg) {
    msg("msg too long");
    return -1;
  }

  err = read_full(fd, (char *)&rbuf[4], len);
  if (err) {
    msg("read() error");
    return err;
  }

  printf("server says: %.*s\n", (int)len, &rbuf[4]);
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

  bool running = true;
  // while (running) {
  //   std::string msg;
  //   std::getline(std::cin, msg);
  //   if (msg == "quit") {
  //     running = false;
  //     break;
  //   }

  //   int32_t err = query(fd, msg.c_str());
  //   if (err) {
  //     running = false;
  //   }
  // }
  std::vector<std::string> query_list = {"hello1", "hello2", "hello3", std::string(k_max_msg, 'z'), "hello5"};
  for (std::string &s : query_list) {
    int32_t err = query(fd, s.c_str());
    if (err) {
      goto L_DONE;
    }
  }

  for (std::string &s : query_list) {
    int32_t err = read_res(fd, s.length());
    if (err) {
      goto L_DONE;
    }
  }

L_DONE:
  close(fd);
  return 0;
}
