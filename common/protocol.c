#include "protocol.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>

ssize_t write_full(int fd, const void *buf, size_t n) {
  const char *p = (const char*)buf;
  size_t off = 0;
  while (off < n) {
    ssize_t w = write(fd, p + off, n - off);
    if (w < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (w == 0) return -1;
    off += (size_t)w;
  }
  return (ssize_t)off;
}

ssize_t read_full(int fd, void *buf, size_t n) {
  char *p = (char*)buf;
  size_t off = 0;
  while (off < n) {
    ssize_t r = read(fd, p + off, n - off);
    if (r < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (r == 0) return 0; // EOF
    off += (size_t)r;
  }
  return (ssize_t)off;
}

int send_msg(int fd, uint32_t type, const void *payload, uint32_t size) {
  msg_hdr_t h;
  h.type = type;
  h.size = size;

  if (write_full(fd, &h, sizeof(h)) < 0) return -1;
  if (size > 0 && payload) {
    if (write_full(fd, payload, size) < 0) return -1;
  }
  return 0;
}

int recv_hdr(int fd, msg_hdr_t *h) {
  ssize_t r = read_full(fd, h, sizeof(*h));
  if (r == 0) return 0;     // EOF
  if (r < 0) return -1;
  return 1;
}
