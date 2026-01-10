#ifndef UNIXSOCK_H
#define UNIXSOCK_H

typedef struct UnixSock {
  int fd;
} unixsock_t;

void unixsock_init(unixsock_t *s);
void unixsock_close(unixsock_t *s);

int unixsock_server_listen(unixsock_t *s, const char *path);   // bind+listen
int unixsock_server_accept(unixsock_t *client, const unixsock_t *server);

int unixsock_client_connect(unixsock_t *s, const char *path);

#endif
