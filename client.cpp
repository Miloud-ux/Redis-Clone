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

static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
  uint32_t len = 4;
  for (const std::string &s : cmd) {
    len += 4 + s.size();
  }
  if (len > k_max_msg) {
    return -1;
  }

  std::vector<uint8_t> wbuf(4 + len);
  memcpy(&wbuf[0], &len, 4);
  uint32_t nstr = cmd.size();
  memcpy(&wbuf[4], &nstr, 4);
  size_t cur = 8;
  for (const std::string &s : cmd) {
    uint32_t slen = s.size();
    memcpy(&wbuf[cur], &slen, 4);
    cur += 4;
    memcpy(&wbuf[cur], s.data(), s.size());
    cur += s.size();
  }

  int32_t err = write_full(fd, (char *)wbuf.data(), 4 + len);
  if (err) {
    return err;
  }
  return 0;
}

static void print_str(const uint8_t *data, uint32_t len) {
  if (len > 64) {
    printf("(str %u bytes) %.*s...\n", len, 64, (const char *)data);
  } else {
    printf("%.*s\n", (int)len, (const char *)data);
  }
}

static int32_t read_res(int fd) {
  std::vector<uint8_t> rbuf(4 + k_max_msg + 1);
  errno = 0;
  int32_t err = read_full(fd, (char *)rbuf.data(), 4);
  if (err) {
    msg(errno == 0 ? "EOF" : "Read() error");
    return err;
  }

  uint32_t len = 0;
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

  size_t cur = 4;
  uint8_t tag = rbuf[cur++];
  if (tag == TAG_NIL) {
    printf("(nil)\n");
  } else if (tag == TAG_STR) {
    uint32_t slen = 0;
    memcpy(&slen, &rbuf[cur], 4);
    cur += 4;
    print_str(&rbuf[cur], slen);
  } else if (tag == TAG_INT) {
    int64_t val = 0;
    memcpy(&val, &rbuf[cur], 8);
    printf("(int %lld)\n", (long long)val);
  } else if (tag == TAG_ARR) {
    uint32_t n = 0;
    memcpy(&n, &rbuf[cur], 4);
    cur += 4;
    printf("(arr %u)\n", n);
    for (uint32_t i = 0; i < n; i++) {
      uint8_t t = rbuf[cur++];
      if (t == TAG_STR) {
        uint32_t slen = 0;
        memcpy(&slen, &rbuf[cur], 4);
        cur += 4;
        printf("  ");
        print_str(&rbuf[cur], slen);
        cur += slen;
      }
    }
  } else if (tag == TAG_ERR) {
    uint32_t code = 0;
    memcpy(&code, &rbuf[cur], 4);
    cur += 4;
    uint32_t elen = 0;
    memcpy(&elen, &rbuf[cur], 4);
    cur += 4;
    printf("(err %u, %.*s)\n", code, (int)elen, (const char *)&rbuf[cur]);
  } else {
    printf("unknown tag %u\n", tag);
  }
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

  std::vector<std::vector<std::string>> tests = {
      {"set", "name", "mello"},
      {"get", "name"},
      {"set", "age", "23"},
      {"get", "age"},
      {"dbsize"},
      {"keys"},
      {"del", "name"},
      {"get", "name"},
      {"keys"},
  };

  for (std::vector<std::string> &cmd : tests) {
    int32_t err = send_req(fd, cmd);
    if (err) {
      break;
    }
    err = read_res(fd);
    if (err) {
      break;
    }
  }

  close(fd);
  return 0;
}