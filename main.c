#include <errno.h>
#include <ev.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PORT 10000
#define BUFFER_SIZE 1024

int connections = 0;

void
close_connection(
  struct ev_loop *loop,
  ev_io *watcher
) {
  ev_io_stop(loop, watcher);
  close(watcher->fd);
  free(watcher);
  connections--;
  printf("client disconnected\n");
  printf("%d client(s) connected\n", connections);
}

void
read_connection(
  struct ev_loop *loop,
  struct ev_io *watcher,
  int revents
) {
  char buffer[BUFFER_SIZE];
  ssize_t read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);

  if (read < 0) {
    if (errno == EAGAIN) {
      return;
    } else {
      printf("read error\n");
      close_connection(loop, watcher);
      return;
    }
  } else if (read == 0) {
    close_connection(loop, watcher);
    return;
  } else {
    printf("read %ld bytes\n", read);
    printf("[");
    for (int i = 0; i < read; i++) {
      printf("%d", buffer[i]);
      if (i != read - 1) {
        printf(",");
      }
    }
    printf("]\n");
    printf("message: \"%s\"\n", buffer);
  }

  // Send message back to the client
  send(watcher->fd, buffer, read, 0);
  memset(buffer, 0, read);
}

void
accept_connection(
  struct ev_loop *loop,
  struct ev_io *watcher,
  int revents
) {
  printf("new connection\n");

  struct sockaddr_in address;
  socklen_t address_length = sizeof(address);

  int socket_fd =
      accept(watcher->fd, (struct sockaddr *)&address, &address_length);

  if (socket_fd < 0) {
    printf("accept error\n");
    return;
  }

  connections++;
  printf("%d connection(s)\n", connections);

  struct ev_io *connection_io = (struct ev_io *)malloc(sizeof(struct ev_io));
  ev_io_init(connection_io, read_connection, socket_fd, EV_READ); // TODO: EV_WRITE?
  ev_io_start(loop, connection_io);
}

int
main()
{
  int socket_fd;
  if ((socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
    printf("failed to create socket\n");
    return -1;
  }

  int option_value = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option_value,
                 sizeof(option_value)) != 0) {
    printf("failed to set socket option: SO_REUSEADDR\n");
    return -1;
  }

  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &option_value,
                 sizeof(option_value)) != 0) {
    printf("failed to set socket option: SO_REUSEPORT\n");
    return -1;
  }

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
    printf("failed to bind\n");
    return -1;
  }

  if (listen(socket_fd, SOMAXCONN) != 0) {
    printf("failed to listen\n");
    return -1;
  }

  struct ev_loop *loop = ev_default_loop(0);

  struct ev_io accept_io;
  ev_io_init(&accept_io, accept_connection, socket_fd, EV_READ);
  ev_io_start(loop, &accept_io);

  ev_run(loop, 0);

  return 0;
}
