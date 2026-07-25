# Simple Guide on Networking and Sockets

## Table of content

<!--toc:start-->

- [Notion on networking](#notion-on-networking)
  - [TCP/IP Model](#tcpip-model)
    - [Ports](#ports)
    - [TCP vs UDP](#tcp-vs-udp)
  - [Sockets](#sockets) - [Definition](#definition) - [Listening](#listening) - [Connection socket](#connection-socket)
  <!--toc:end-->

## TCP/IP Model

The OSI Model is an old theoretical model that doesn't represent real world, instead software uses the **TCP/IP** model :
Application -> Transport layer (TCP/UDP) -> IP layer -> Link layer (below IP)
High layer <-----------------------------------------------------> Low layer

In network programming we usually deal with this tuple: (src_ip, src_port, dst_ip, dst_port).

### Ports

**Ports** are on top of the IP layer and typically apps don't talk to the ip layer since it doesn't know which
data belongs to which program thus ports are needed.

### TCP vs UDP

TCP is a continuous stream of bytes that is reliable and ordered which means we guarentee the bytes arrive in the correct order handling cases of syncing, loosing data...etc but the downside is we don't know the length of each byte sequence to correctly decode it.

UDP is a message based protocol that guarentees that messages have a known length and boundary but it's unreliable ie: packets can be lost, out of order...etc

*Redis*, HTTP/1.1, and most RPC protocols are request-response protocols. To build them we need reliable request-message protocol, so we either build a message-response layer on top of TCP or solve UDP problems (the first is easier).

---

## Sockets

### Definition

A socket is a handle (int) that refers usually to a connection. Instead of handling sensetive permissios to the program to use the connection, the OS uses that handle and maps it to the correct socket slot in the global socket table in the kernel space. This definition is abstract and it boils down to sockets being a **bridge** that abstracts away the connection details on the OS level.

In reality sockets are a complex struct managed by the OS and the fd is just a local handle for each process, so the 'handle' is not the actual socket itself but the fd. The OS links that local fd to it's appropriate socket struct using a **global fd table** which then the socket is manipulated using system calls.

In UNIX this handle is known as **file descriptor** (fd) and it's local to the process. The name 'file descriptor'is just a name that has nothing to do with files, it's a convention to reference UNIX's philosohpy (everythinng is a file) so the fd can refer to a socket, file, pipe...and more.


> [!note] Docs
> To get documentation about sockets you use `man read.2`, `man socket.2` the '.2' refers to the second section
> of the manual (the syscall section) since socket API methods on Linux are syscalls.
> `man ip.7` tells you how to create TCP/UDP sockets and the required #includes.

Sockets have different types : **listening sockets** and **connection sockets**.

### Listening

Listening is telling the OS that this app will receive data in the future through an IP:port you choose. To create a **listening
socket** we use the `socket() -> bind()->listen()` api calls, then `accept()` to accept the incoming connection,  Finally `close()`.

### Connection socket

Connecting is done using `socket()->connect()` api call, and is closed using `close()`.

`socket()` creates a typeless socket whom's type (listening or connection) and actual creation is done after the `listen()` or `connect()` call.

### Socket options
`setsockopt()` sets additional socket options such as **SO_REUSEADDR** which allows the server to use the same
Ip:port after restarting wihtout crashing the server bypassing the **TIME_WAIT** safety rule, and for this
reason most webservers (like Redis) have this option on despite the risk. 

### Sockaddr
C doesn't support OOP (thankfully) so in order to achieve simiar properties like **fucntion overloading** and
**inheritance** we cast to `sockaddr`  which is a 'generic' socket type that is used only for casting. But how
would the OS know the type of the socket you might askt ? The answer is by knowing the **size** of the socket
struct.

Looking at the socketAPI, sockaddr is like void type and it has no use really so im not sure about the OOP part.
.
*Example*:
```C
struct sockaddr_in addr = {...}; // IPV4 socket
bind(fd, (const struct sockaddr*)&addr, sizeof(addr));
```
