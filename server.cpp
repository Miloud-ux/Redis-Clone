#include <arpa/inet.h>
#include <cerrno>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>
#include <vector>

#include "server.h"
static void fd_set_nb(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0); // IPV4 TCP socket
  if (fd < 0) {
    die("socket()");
  }
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = htonl(INADDR_ANY); // to fix endianess
  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));

  if (rv) {
    die("bind()");
  }

  // 40% (size of the queue) doesn't matter on linux anyway.
  fd_set_nb(fd);
  rv = listen(fd, SOMAXCONN);

  std::vector<struct pollfd> poll_args;
  std::vector<Conn *> fd2Conn;

  while (1) {
    // == Prepare the args for poll() ==
    poll_args.clear();
    // push the listening socket in the first pos
    poll_args.push_back({fd, POLLIN, 0});

    // rest of socket
    for (Conn *conn : fd2Conn) {
      if (!conn) {
        continue;
      }

      struct pollfd pfd = {conn->fd, POLLERR, 0};
      if (conn->want_read) {
        pfd.events |= POLLIN;
      }
      if (conn->want_write) {
        pfd.events |= POLLOUT;
      }

      poll_args.push_back(pfd);
    }

    // == Wait for readiness ==
    int rv = poll(poll_args.data(), poll_args.size(), 0);
    if (rv < 0 && errno == EINTR) {
      continue; // not an error
    }

    if (rv < 0) {
      die("poll");
    }

    //  == Accept new connection ==
    if (poll_args[0].revents) {
      // reads are treated like accpets
      if (Conn *conn = handle_accept(fd)) {
        if (fd2Conn.size() <= (size_t)conn->fd) {
          fd2Conn.resize(conn->fd + 1);
        }
        fd2Conn[conn->fd] = conn;
      }
    }

    // == Invoke app callbacks ==
    for (size_t i = 1; i < poll_args.size(); i++) {
      short ready = poll_args[i].revents;

      Conn *conn = fd2Conn[poll_args[i].fd];
      if (ready & POLLIN) {
        handle_read(conn);
      }

      if (ready & POLLOUT) {
        handle_write(conn);
      }

      if (ready & POLLERR || conn->want_close) {
        (void)close(conn->fd);
        fd2Conn[conn->fd] = NULL;
        delete conn;
      }
    }
  }

  return 0;
}
