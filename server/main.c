#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <common/unixsock.h>
#include <common/protocol.h>
#include <common/util.h>
#include "world.h"
#include "snake.h"

#define TICK_MS 200

typedef enum { MODE_STAND = 1, MODE_CAS = 2 } game_mode_t;
typedef enum { WORLD_PRAZDNY = 1, WORLD_PREKAZKY = 2 } world_type_t;

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s <game_id>\n", prog);
}

static void sleep_ms(long ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000 * 1000;
  nanosleep(&ts, NULL);
}

static int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv) {
  if (argc < 7) {
    fprintf(stderr, "Usage: %s <game_id> <w> <h> <mode 1|2> <world 1|2> <time_sec>\n", argv[0]);
    return 1;
  }

  const char *game_id = argv[1];
  int W = atoi(argv[2]);
  int H = atoi(argv[3]);
  int mode_i = atoi(argv[4]);
  int world_i = atoi(argv[5]);
  int time_sec = atoi(argv[6]);

  game_mode_t mode = (mode_i == 2) ? MODE_CAS : MODE_STAND;
  world_type_t world_type = (world_i == 2) ? WORLD_PREKAZKY : WORLD_PRAZDNY;

  if (W < 10) W = 10;
  if (H < 10) H = 10;
  if (time_sec < 0) time_sec = 0;

  char sock_path[108];
  if (util_snprintf(sock_path, sizeof(sock_path), "/tmp/snake_%s.sock", game_id) != 0) {
    fprintf(stderr, "Server: failed to build socket path\n");
    return 1;
  }

  unixsock_t server, client;
  unixsock_init(&server);
  unixsock_init(&client);

  if (unixsock_server_listen(&server, sock_path) != 0) {
    return 1;
  }

  //printf("Server listening on %s (pid=%d)\n", sock_path, getpid());
  fflush(stdout);

  if (unixsock_server_accept(&client, &server) != 0) {
    unixsock_close(&server);
    return 1;
  }

  // nastav socket na non-blocking
  int flags = fcntl(client.fd, F_GETFL, 0);
  fcntl(client.fd, F_SETFL, flags | O_NONBLOCK);

  //printf("Server accepted client.\n");
  fflush(stdout);

  uint32_t tick = 0;
  uint32_t score = 0;
  int running = 1;
  int paused = (uint8_t)paused;
  int resume_countdown = (uint8_t)resume_countdown;
  uint64_t last_countdown_ms = 0;
  
  //   vytvorenie sveta
  
    world_t world;
    if (world_init(&world, W, H) != 0) {
    fprintf(stderr, "server: world_init failed\n");
      return 1;
    }

    unsigned seed = (unsigned)getpid();

    if (world_type == 2) {
      do {
        world_generate_obstacles(&world, 0.05f, seed++);
      } while (!world_is_connected_bfs(&world));
    } else {
    // prázdny svet bez borderu
    world_clear(&world, ' ');
    }

    world_place_fruit(&world);


   //  vztvorenie hada
 

    snake_t snake;
    if (snake_init(&snake, W * H) != 0) {
      fprintf(stderr, "server: snake_init failed\n");
      world_destroy(&world);
      return 1;
    }
    snake_spawn(&snake, &world);

  uint64_t game_start_ms = util_now_ms();
  // Tick loop: každých 100 ms pošli STATE
while (running) {

    uint64_t now = util_now_ms();
    uint32_t game_limit_ms = (mode == MODE_CAS) ? (uint32_t)time_sec * 1000u : 0;
    uint32_t time_left_ms = 0;


  // spracovanie inputu
  for (;;) {
    msg_hdr_t ih;
    int hr = recv_hdr(client.fd, &ih);

    if (hr == 0) {
      // klient zavrel spojenie
      running = 0;
      break;
    }

    if (hr < 0) {
      // nic sa nedeje, pokracujeme
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      running = 0;
      break;
    }
    if (ih.type == MSG_INPUT && ih.size == sizeof(msg_input_t)) {
      msg_input_t in;
      if (read_full(client.fd, &in, sizeof(in)) <= 0) {
        running = 0;
        break;
      }
      if (in.quit) {
        running = 0;
        break;
      }
      if (in.pause_toggle) {
        if (!paused) {
          paused = 1;
        } else {
          // odpauznutie sputi 3 sekund odpocet
          paused = 0;
          resume_countdown = 3;
        }
      }
      //pohyb hada
      if (in.has_dir) {
        snake_set_dir(&snake, (dir_t)in.dir);
      }
    } else {
      char tmp[256];
      uint32_t left = ih.size;
      while (left > 0) {
        uint32_t chunk = left > sizeof(tmp) ? (uint32_t)sizeof(tmp) : left;
        if (read_full(client.fd, tmp, chunk) <= 0) {
          running = 0;
          break;
        }
        left -= chunk;
      }
    }
  }
  if (mode == MODE_CAS) {
        uint64_t elapsed = now - game_start_ms;
        if (elapsed >= game_limit_ms) {
            const char *msg = "Time up!";
            send_msg(client.fd, MSG_TEXT, msg, (uint32_t)strlen(msg) + 1);
            running = 0;
            break;
        } else {
            time_left_ms = (uint32_t)(game_limit_ms - elapsed);
        }
    } else {
        time_left_ms = 0;
    }
  // pohyb hada
    if (paused) {
      // nis  had stoji a caka
    } else if (resume_countdown > 0) {
      if (last_countdown_ms == 0)
            last_countdown_ms = now;

        if (now - last_countdown_ms >= 1000) {
            resume_countdown--;
            last_countdown_ms = now;
        }
    } else {
      // game over, skoncenie hry
      if (snake_step(&snake, &world, &score, world_type) != 0) {
        char buf[64];
        snprintf(buf, sizeof(buf),
           "Score: %u, Time: %us",
           score, (unsigned)((now - game_start_ms) / 1000));

        send_msg(client.fd, MSG_TEXT, "Game over!", (uint32_t)strlen("Game over!") + 1);
        send_msg(client.fd, MSG_TEXT, buf, (uint32_t)strlen(buf) + 1);

        running = 0;
        break;
      }
    }

  // sprav mriezku

  char grid[W * H];
  memcpy(grid, world.cells, (size_t)(W * H));


   // posiela spravy

  msg_state_hdr_t sh;
  sh.w = W;
  sh.h = H;
  sh.tick = tick;
  sh.score = score;
  sh.time_left_ms = time_left_ms;
  sh.paused = (uint8_t)paused;
  sh.resume_countdown = (uint8_t)resume_countdown;
  sh._pad = 0;

  msg_hdr_t out;
  out.type = MSG_STATE;
  out.size = (uint32_t)sizeof(sh) + (uint32_t)(W * H);

  if (write_full(client.fd, &out, sizeof(out)) < 0) {
    running = 0;
    break;
  }
  if (write_full(client.fd, &sh, sizeof(sh)) < 0) {
    running = 0;
    break;
  }
  if (write_full(client.fd, grid, (size_t)(W * H)) < 0) {
    running = 0;
    break;
  }

  // tick a cakanie
   tick++;
  sleep_ms(TICK_MS);
}


  unixsock_close(&client);
  unixsock_close(&server);
  unlink(sock_path);
  world_destroy(&world);
  snake_destroy(&snake);
  return 0;
}

