#include "snake.h"
#include <stdlib.h>
#include <string.h>

#define EMPTY ' '
#define WALL  '#'
#define FRUIT '*'
#define SNAKE 'o'
#define HEAD  '@'

static int idx(const world_t *w, int x, int y) { return y * w->w + x; }

static int is_opposite(dir_t a, dir_t b) {
  return (a == DIR_UP && b == DIR_DOWN) ||
         (a == DIR_DOWN && b == DIR_UP) ||
         (a == DIR_LEFT && b == DIR_RIGHT) ||
         (a == DIR_RIGHT && b == DIR_LEFT);
}

int snake_init(snake_t *s, int max_cells) {
  s->body = (pos_t*)malloc((size_t)max_cells * sizeof(pos_t));
  if (!s->body) return -1;
  s->capacity = max_cells;
  s->len = 0;
  s->dir = DIR_RIGHT;
  s->grow = 0;
  return 0;
}

void snake_destroy(snake_t *s) {
  free(s->body);
  s->body = NULL;
}

void snake_spawn(snake_t *s, const world_t *w) {
  // nájdi náhodné voľné miesto
  int size = (int)w->w * (int)w->h;
  for (int tries = 0; tries < 10000; tries++) {
    int i = rand() % size;
    if (w->cells[i] == EMPTY) {
      s->len = 3;
      int x = i % w->w;
      int y = i / w->w;

      // telo dozadu doľava (aby to bolo vnútri)
      s->body[0] = (pos_t){x, y};
      s->body[1] = (pos_t){x - 1, y};
      s->body[2] = (pos_t){x - 2, y};
      s->dir = DIR_RIGHT;
      s->grow = 0;
      return;
    }
  }

  // fallback
  s->len = 1;
  s->body[0] = (pos_t){1, 1};
  s->dir = DIR_RIGHT;
  s->grow = 0;
}

void snake_set_dir(snake_t *s, dir_t d) {
  // zakáž otočenie o 180° (klasické hadík pravidlo)
  if (!is_opposite(s->dir, d)) s->dir = d;
}

static void render_snake_into_world(const snake_t *s, world_t *w) {
  // vymaž staré snake znaky (len 'o' a '@')
  int size = (int)w->w * (int)w->h;
  for (int i = 0; i < size; i++) {
    if (w->cells[i] == SNAKE || w->cells[i] == HEAD) w->cells[i] = EMPTY;
  }

  // vykresli nové
  for (int i = s->len - 1; i >= 0; i--) {
    int x = s->body[i].x, y = s->body[i].y;
    w->cells[idx(w, x, y)] = (i == 0) ? HEAD : SNAKE;
  }
}

int snake_step(snake_t *s, world_t *w, uint32_t *score) {
  // vypočítaj novú hlavu
  int nx = s->body[0].x;
  int ny = s->body[0].y;

  if (s->dir == DIR_UP) ny--;
  else if (s->dir == DIR_DOWN) ny++;
  else if (s->dir == DIR_LEFT) nx--;
  else if (s->dir == DIR_RIGHT) nx++;

  if (world_type == WORLD_PRAZDNY) {
    if (nx < 0) nx = w->w - 1;
    if (nx >= (int)w->w) nx = 0;
    if (ny < 0) ny = w->h - 1;
    if (ny >= (int)w->h) ny = 0;
  } else {
    // s prekazkami koniec ak narazi
    if (nx < 0 || ny < 0 || nx >= (int)w->w || ny >= (int)w->h) return -1;
  }


  char cell = w->cells[idx(w, nx, ny)];

  // kolízia
  if (cell == WALL || cell == SNAKE) return -1;

  // ovocie
  if (cell == FRUIT) {
    (*score)++;
    s->grow += 1;
    // nové ovocie dáme až po pohybe
  }

  // posuň telo: od konca k 1
  if (s->len < s->capacity && s->grow > 0) {
    // rast: predĺžime len o 1
    for (int i = s->len; i > 0; i--) s->body[i] = s->body[i - 1];
    s->len++;
    s->grow--;
  } else {
    // nerastie: posúvame a posledný odpadne
    for (int i = s->len - 1; i > 0; i--) s->body[i] = s->body[i - 1];
  }

  s->body[0] = (pos_t){nx, ny};

  // keď zjedol ovocie, polož nové
  if (cell == FRUIT) {
    world_place_fruit(w);
  }

  render_snake_into_world(s, w);
  return 0;
}
