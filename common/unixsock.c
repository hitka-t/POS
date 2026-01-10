#include "unixsock.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static int make_unix_addr(struct sockaddr_un *addr, const char *path) {
  memset(addr, 0, sizeof(*addr));
  addr->sun_family = AF_UNIX;
  // Pozor na dĺžku
  if (!path) return -1;
  size_t n = strlen(path);
  if (n >= sizeof(addr->sun_path)) return -1;
  strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1);
  return 0;
}

void unixsock_init(unixsock_t *s) {
  s->fd = -1;
}

void unixsock_close(unixsock_t *s) {
  if (s && s->fd >= 0) {
    close(s->fd);
    s->fd = -1;
  }
}

int unixsock_server_listen(unixsock_t *s, const char *path) {
  if (!s || !path) return -1;

  s->fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s->fd < 0) {
    perror("unixsock_server_listen: socket");
    return -1;
  }

  // aby šlo opakovane spustiť
  unlink(path);

  struct sockaddr_un addr;
  if (make_unix_addr(&addr, path) != 0) {
    fprintf(stderr, "unixsock_server_listen: bad path\n");
    unixsock_close(s);
    return -1;
  }

  if (bind(s->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("unixsock_server_listen: bind");
    unixsock_close(s);
    return -1;
  }

  if (listen(s->fd, 1) < 0) {
    perror("unixsock_server_listen: listen");
    unixsock_close(s);
    return -1;
  }

  return 0;
}

int unixsock_server_accept(unixsock_t *client, const unixsock_t *server) {
  if (!client || !server || server->fd < 0) return -1;

  client->fd = accept(server->fd, NULL, NULL);
  if (client->fd < 0) {
    perror("unixsock_server_accept: accept");
    return -1;
  }
  return 0;
}

int unixsock_client_connect(unixsock_t *s, const char *path) {
  if (!s || !path) return -1;

  s->fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s->fd < 0) {
    perror("unixsock_client_connect: socket");
    return -1;
  }

  struct sockaddr_un addr;
  if (make_unix_addr(&addr, path) != 0) {
    fprintf(stderr, "unixsock_client_connect: bad path\n");
    unixsock_close(s);
    return -1;
  }

  if (connect(s->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    fprintf(stderr, "unixsock_client_connect: connect(%s): %s\n", path, strerror(errno));
    unixsock_close(s);
    return -1;
  }

  return 0;
}
