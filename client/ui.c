#include "ui.h"
#include <ncurses.h>
#include <string.h>
#include <common/protocol.h>

void ui_init(void) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE); // neblokuje
  curs_set(0);

  clear();
  mvprintw(0, 0, "Snake MVP (arrows=direction, p=pause, q=quit)");
  mvprintw(2, 0, "Status: ");
  mvprintw(4, 0, "World: ");
  refresh();
}

void ui_shutdown(void) {
  endwin();
}

msg_input_t ui_read_input(void) {
  msg_input_t in;
  memset(&in, 0, sizeof(in));

  int ch = getch();
  if (ch == ERR) return in; // nič nestlačené

  switch (ch) {
    case KEY_UP:    in.has_dir = 1; in.dir = DIR_UP; break;
    case KEY_RIGHT: in.has_dir = 1; in.dir = DIR_RIGHT; break;
    case KEY_DOWN:  in.has_dir = 1; in.dir = DIR_DOWN; break;
    case KEY_LEFT:  in.has_dir = 1; in.dir = DIR_LEFT; break;
    case 'p':       in.pause_toggle = 1; break;
    case 'q':       in.quit = 1; break;
    default: break;
  }
  return in;
}

void ui_show_status(const char *text) {
  mvprintw(3, 0, "Server says: %-70s", text ? text : "(null)");
  refresh();
}

void ui_draw_world(uint16_t w, uint16_t h, const char *grid,
                   uint32_t score, uint32_t tick,
                   uint8_t paused, uint8_t resume_countdown) {
  mvprintw(1, 0, "Score: %u   Tick: %u", score, tick);
  if (paused) {
    mvprintw(2, 0, "PAUSED  (press 'p' to resume)                 ");
  } else if (resume_countdown > 0) {
    mvprintw(2, 0, "Resuming in: %u                               ",
           (unsigned)resume_countdown);
  } else {
    mvprintw(2, 0, "                                              ");
  }

  int start_y = 5;
  for (uint16_t y = 0; y < h; y++) {
    move(start_y + (int)y, 0);
    for (uint16_t x = 0; x < w; x++) {
      if (y == 0 || y == h - 1 || x == 0 || x == w - 1) {
        addch('#');
      } else {
        addch(grid[y * w + x]);
      }
    }
  }
  refresh();
}
