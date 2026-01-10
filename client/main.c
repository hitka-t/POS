#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

#include <common/unixsock.h>
#include <common/protocol.h>
#include <common/util.h>

#include "ui.h"

/* =========================
   Kontext klienta (zdieľaný medzi vláknami)
   ========================= */
typedef struct {
  int fd;
  atomic_int running;
} client_ctx_t;

/* =========================
   Input thread – číta klávesy a posiela serveru
   ========================= */
static void *input_thread_fn(void *arg) {
  client_ctx_t *ctx = arg;
  msg_input_t last;
  memset(&last, 0, sizeof(last));

  while (atomic_load(&ctx->running)) {
    msg_input_t in = ui_read_input();

    // nič nestlačené → krátko spi
    if (!in.has_dir && !in.pause_toggle && !in.quit) {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 10 * 1000 * 1000; // 10 ms
      nanosleep(&ts, NULL);
      continue;
    }

    // neposielaj stále ten istý smer
    int same_dir =
      in.has_dir && last.has_dir &&
      in.dir == last.dir &&
      !in.pause_toggle &&
      !in.quit;

    if (same_dir) continue;

    if (send_msg(ctx->fd, MSG_INPUT, &in, (uint32_t)sizeof(in)) != 0) {
      atomic_store(&ctx->running, 0);
      break;
    }

    last = in;

    if (in.quit) {
      atomic_store(&ctx->running, 0);
      break;
    }
  }
  return NULL;
}

/* =========================
   Receive/render thread – prijíma dáta zo servera a kreslí
   ========================= */
static void *recv_thread_fn(void *arg) {
  client_ctx_t *ctx = arg;

  while (atomic_load(&ctx->running)) {
    msg_hdr_t h;
    int rh = recv_hdr(ctx->fd, &h);
    if (rh <= 0) {
      ui_show_status("Disconnected from server.");
      atomic_store(&ctx->running, 0);
      break;
    }

    if (h.type == MSG_TEXT && h.size > 0 && h.size < 4096) {
      char buf[4096];
      if (read_full(ctx->fd, buf, h.size) <= 0) {
        atomic_store(&ctx->running, 0);
        break;
      }
      buf[sizeof(buf) - 1] = '\0';
      ui_show_status(buf);
    }
    else if (h.type == MSG_STATE && h.size >= sizeof(msg_state_hdr_t)) {
      msg_state_hdr_t sh;
      if (read_full(ctx->fd, &sh, sizeof(sh)) <= 0) {
        atomic_store(&ctx->running, 0);
        break;
      }

      uint32_t grid_bytes = (uint32_t)sh.w * (uint32_t)sh.h;
      uint32_t remaining = h.size - (uint32_t)sizeof(sh);
      if (remaining != grid_bytes || grid_bytes > 200000) {
        ui_show_status("Bad STATE payload.");
        // preskoč zvyšok (ak nejaký)
        char tmp[256];
        uint32_t left = remaining;
        while (left > 0) {
          uint32_t chunk = left > sizeof(tmp) ? (uint32_t)sizeof(tmp) : left;
          if (read_full(ctx->fd, tmp, chunk) <= 0) break;
          left -= chunk;
        }
        continue;
      }

      // grid buffer (stack OK pre malé rozmery)
      char *grid = malloc(grid_bytes);
      if (!grid) {
        ui_show_status("malloc failed for grid");
        atomic_store(&ctx->running, 0);
        break;
      }

      if (read_full(ctx->fd, grid, grid_bytes) <= 0) {
        free(grid);
        atomic_store(&ctx->running, 0);
        break;
      }

      ui_draw_world(sh.w, sh.h, grid, sh.score, sh.tick, sh.paused, sh.resume_countdown);
      free(grid);
    }
    else {
      // preskoč neznámy payload
      char tmp[256];
      uint32_t left = h.size;
      while (left > 0) {
        uint32_t chunk = left > sizeof(tmp) ? (uint32_t)sizeof(tmp) : left;
        if (read_full(ctx->fd, tmp, chunk) <= 0) {
          atomic_store(&ctx->running, 0);
          break;
        }
        left -= chunk;
      }
    }
  }

  return NULL;
}

/* =========================
   Spustenie servera (fork + exec)
   ========================= */
static int spawn_server(const char *game_id) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("client: fork");
    return -1;
  }
  if (pid == 0) {
    execl("./server", "./server", game_id, (char *)NULL);
    perror("client: exec server");
    _exit(127);
  }
  // parent NEČAKÁ (P5 – server žije samostatne)
  return 0;
}

/* =========================
   main
   ========================= */
int main(void) {
  /* --------- vytvor game_id --------- */
  char game_id[64];
  unsigned int pid = (unsigned int)getpid();
  unsigned int r = util_rand_u32();

  if (util_snprintf(game_id, sizeof(game_id), "%u_%u", pid, r) != 0) {
    fprintf(stderr, "client: failed to create game_id\n");
    return 1;
  }

  /* --------- spusti server --------- */
  if (spawn_server(game_id) != 0)
    return 1;

  /* --------- socket path --------- */
  char sock_path[108];
  if (util_snprintf(sock_path, sizeof(sock_path),
                    "/tmp/snake_%s.sock", game_id) != 0) {
    fprintf(stderr, "client: failed to build socket path\n");
    return 1;
  }

  /* --------- pripojenie na server (retry loop) --------- */
  unixsock_t s;
  unixsock_init(&s);

  int connected = 0;
  for (int i = 0; i < 50; i++) {
    // každý pokus začni s čistým fd
    unixsock_close(&s);
    unixsock_init(&s);

    if (unixsock_client_connect(&s, sock_path) == 0) {
      connected = 1;
      break;
    }

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 50 * 1000 * 1000; // 50 ms
    nanosleep(&ts, NULL);
  }

  if (!connected) {
    fprintf(stderr, "client: cannot connect to %s\n", sock_path);
    return 1;
  }

  /* --------- UI init --------- */
  ui_init();
  ui_show_status("Connected to server.");

  /* --------- vlákna --------- */
  client_ctx_t ctx;
  ctx.fd = s.fd;
  atomic_init(&ctx.running, 1);

  pthread_t th_input, th_recv;
  pthread_create(&th_recv, NULL, recv_thread_fn, &ctx);
  pthread_create(&th_input, NULL, input_thread_fn, &ctx);

  /* čakáme, kým input thread neskončí (q) */
  pthread_join(th_input, NULL);
  atomic_store(&ctx.running, 0);
  pthread_join(th_recv, NULL);

  /* --------- cleanup --------- */
  ui_shutdown();
  unixsock_close(&s);
  return 0;
}

