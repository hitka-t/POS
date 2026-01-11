#include "snake.h"
#include <stdlib.h>
#include <string.h>

#define EMPTY ' '
#define WALL  '#'
#define FRUIT '*'
#define SNAKE 'o'
#define HEAD  '@'
//prepocita suradnice
static int idx(const world_t *w, int x, int y) { return y * w->w + x; }
//kontrolujem aby sa had neotocil o 180 stupnov ak stlacim ryzchlo, aby nedoslo ku kolizii
static int is_opposite(dir_t a, dir_t b) {
  return (a == DIR_UP && b == DIR_DOWN) ||
         (a == DIR_DOWN && b == DIR_UP) ||
         (a == DIR_LEFT && b == DIR_RIGHT) ||
         (a == DIR_RIGHT && b == DIR_LEFT);
}
//inicialiyacia hada, alokujem pamat
int snake_init(snake_t *s, int max_cells) {
  s->body = (pos_t*)malloc((size_t)max_cells * sizeof(pos_t));
  if (!s->body) return -1;
  s->capacity = max_cells;
  s->len = 0;
  s->dir = DIR_RIGHT;
  s->grow = 0;
  return 0;
}
//znicenie hada na konci hry, dealokacia pamate
void snake_destroy(snake_t *s) {
  free(s->body);
  s->body = NULL;
}
//spawne hada
void snake_spawn(snake_t *s, const world_t *w) {
  // nájdi náhodné voľné miesto
  int size = (int)w->w * (int)w->h;
  for (int tries = 0; tries < 10000; tries++) {
    int i = rand() % size;
    if (w->cells[i] == EMPTY) {
      s->len = 3;
      int x = i % w->w;
      int y = i / w->w;

      s->body[0] = (pos_t){x, y};
      s->body[1] = (pos_t){x - 1, y};
      s->body[2] = (pos_t){x - 2, y};
      s->dir = DIR_RIGHT;
      s->grow = 0;
      return;
    }
  }

  s->len = 1;
  s->body[0] = (pos_t){1, 1};
  s->dir = DIR_RIGHT;
  s->grow = 0;
}

void snake_set_dir(snake_t *s, dir_t d) {
  if (!is_opposite(s->dir, d)) s->dir = d;
}
//vymaze stareho hada a pripravi noveho na vykreslenie
static void render_snake_into_world(const snake_t *s, world_t *w) {
  // vymaz stare snake znaky (len 'o' a '@')
  int size = (int)w->w * (int)w->h;
  for (int i = 0; i < size; i++) {
    if (w->cells[i] == SNAKE || w->cells[i] == HEAD) w->cells[i] = EMPTY;
  }

  // vykresli nove
  for (int i = s->len - 1; i >= 0; i--) {
    int x = s->body[i].x, y = s->body[i].y;
    w->cells[idx(w, x, y)] = (i == 0) ? HEAD : SNAKE;
  }
}
//jeden herny krok hada
int snake_step(snake_t *s, world_t *w, uint32_t *score, int world_type) {
  // vypocitaj novu hlavu
  int nx = s->body[0].x;
  int ny = s->body[0].y;

  if (s->dir == DIR_UP) ny--;
  else if (s->dir == DIR_DOWN) ny++;
  else if (s->dir == DIR_LEFT) nx--;
  else if (s->dir == DIR_RIGHT) nx++;

  if (world_type == 1) {
    if (nx < 0) nx = w->w - 1;
    if (nx >= (int)w->w) nx = 0;
    if (ny < 0) ny = w->h - 1;
    if (ny >= (int)w->h) ny = 0;
  } else {
    // s prekazkami koniec ak narazi
    if (nx < 0 || ny < 0 || nx >= (int)w->w || ny >= (int)w->h) return -1;
  }


  char cell = w->cells[idx(w, nx, ny)];

  // stret so stenou
  if (cell == WALL || cell == SNAKE) return -1;

  // ovocie
  if (cell == FRUIT) {
    (*score)++;
    s->grow += 1;
  }

  // posuvam telo
  if (s->len < s->capacity && s->grow > 0) {
    // rastie telo o 
    for (int i = s->len; i > 0; i--) s->body[i] = s->body[i - 1];
      s->len++;
      s->grow--;
  } else {
    // nerastie telo
    for (int i = s->len - 1; i > 0; i--) s->body[i] = s->body[i - 1];
  }

  s->body[0] = (pos_t){nx, ny};

  // ked zjem dam nove
  if (cell == FRUIT) {
    world_place_fruit(w);
  }

  render_snake_into_world(s, w);
  return 0;
}
