#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>

#include <common/unixsock.h>
#include <common/protocol.h>
#include <common/util.h>

#include "ui.h"

typedef struct {
  int fd;
  atomic_int running;
} client_ctx_t;


  // citanie z klavesnice
  static void *input_thread_fn(void *arg) {
  client_ctx_t *ctx = arg;
  msg_input_t last;
  memset(&last, 0, sizeof(last));

  while (atomic_load(&ctx->running)) {
    msg_input_t in = ui_read_input();

    // caka ci sa stlacilo nieco
    if (!in.has_dir && !in.pause_toggle && !in.quit) {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 10 * 1000 * 1000; // 10 ms
      nanosleep(&ts, NULL);
      continue;
    }

    // posielanie stale toho isteho smeru
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

  //  kresli a hybe hadom
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
        char tmp[256];
        uint32_t left = remaining;
        while (left > 0) {
          uint32_t chunk = left > sizeof(tmp) ? (uint32_t)sizeof(tmp) : left;
          if (read_full(ctx->fd, tmp, chunk) <= 0) break;
          left -= chunk;
        }
        continue;
      }

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
typedef struct {
  int w, h;
  int mode;      // 1=standard, 2=cas
  int world;     // 1=prazdny, 2=prekazky
  int time_sec;  // 0 pre standard
} menu_choice_t;
//uvidne menu na vyber modu
static int menu_choose_int(const char *title, const char *a, const char *b, const char *c) {
  clear();
  mvprintw(0, 0, "%s", title);
  mvprintw(2, 0, "1) %s", a);
  mvprintw(3, 0, "2) %s", b);
  if (c) mvprintw(4, 0, "3) %s", c);
  mvprintw(6, 0, "zadaj 1/2%s", c ? "/3" : "");
  refresh();

  for (;;) {
    int ch = getch();
    if (ch == '1') return 1;
    if (ch == '2') return 2;
    if (c && ch == '3') return 3;
  }
}
// menu na vytvorenie hry
static menu_choice_t menu_new_game(void) {
  menu_choice_t m;
  memset(&m, 0, sizeof(m));

  int wsel = menu_choose_int(
      "Vyber svet (Prekazky/prazdny)",
      "1 == pradny = ked prides ku kraju mapy objavis sa na druhej strane",
      "2 == Prekazky = ked narazis koniec hry",
      NULL);
  m.world = (wsel == 2) ? 2 : 1;

  int msel = menu_choose_int(
      "Vyber mod: ",
      "Standard",
      "Cas",
      NULL);
  m.mode = (msel == 2) ? 2 : 1;

  int rsel = menu_choose_int(
      "Vyber velkost hracieho gridu",
      "20 x 15",
      "30 x 20",
      "40 x 25");
  if (rsel == 1) { m.w = 20; m.h = 15; }
  else if (rsel == 2) { m.w = 30; m.h = 20; }
  else { m.w = 40; m.h = 25; }

  if (m.mode == 2) {
    int tsel = menu_choose_int(
        "Vyber dlzku hry",
        "30 sec",
        "60 sec",
        "120 sec");
    if (tsel == 1) m.time_sec = 30;
    else if (tsel == 2) m.time_sec = 60;
    else m.time_sec = 120;
  } else {
    m.time_sec = 0;
  }

  return m;
}


  // spustenie servera fork + exec
static int spawn_server(const char *game_id, const menu_choice_t *m) {
  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    char w_str[16], h_str[16], mode_str[16], world_str[16], time_str[16];
    snprintf(w_str, sizeof(w_str), "%d", m->w);
    snprintf(h_str, sizeof(h_str), "%d", m->h);
    snprintf(mode_str, sizeof(mode_str), "%d", m->mode);
    snprintf(world_str, sizeof(world_str), "%d", m->world);
    snprintf(time_str, sizeof(time_str), "%d", m->time_sec);

    execl("./server", "./server",
          game_id, w_str, h_str, mode_str, world_str, time_str,
          (char *)NULL);

    _exit(127);
  }
  return 0;
}


int main(void) {
  ui_init();
  menu_choice_t choice = menu_new_game();

  //vytvor game_id
  char game_id[64];
  unsigned int pid = (unsigned int)getpid();
  unsigned int r = util_rand_u32();

  if (util_snprintf(game_id, sizeof(game_id), "%u_%u", pid, r) != 0) {
    fprintf(stderr, "nepodarilo sa vztvorit game id\n");
    return 1;
  }

  // spusti server
  if (spawn_server(game_id, &choice) != 0) {
    ui_shutdown();
    return 1;
  }


  // socket path
  char sock_path[108];
  if (util_snprintf(sock_path, sizeof(sock_path),
                    "/tmp/snake_%s.sock", game_id) != 0) {
    fprintf(stderr, "nepodarilo sa vztvorit cestu\n");
    return 1;
  }

  ui_show_status("pripojene");

  // pripojenie na server
  unixsock_t s;
  unixsock_init(&s);

  int connected = 0;
  for (int i = 0; i < 50; i++) {
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
    fprintf(stderr, "nepodarilo sa pripojit %s\n", sock_path);
    return 1;
  }

  // vlakno
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

  // cistenie
  ui_shutdown();
  unixsock_close(&s);
  return 0;
}

