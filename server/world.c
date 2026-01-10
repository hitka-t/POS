#include "world.h"
#include <stdlib.h>
#include <string.h>

#define EMPTY ' '
#define WALL  '#'
#define FRUIT '*'
#define OBSTACLE 'X'

static inline int idx(const world_t *w, int x, int y) { return y * w->w + x; }

int world_init(world_t *w, uint16_t width, uint16_t height) {
  w->w = width; w->h = height;
  w->cells = (char*)malloc((size_t)width * (size_t)height);
  if (!w->cells) return -1;
  memset(w->cells, EMPTY, (size_t)width * (size_t)height);
  return 0;
}

void world_destroy(world_t *w) {
  free(w->cells);
  w->cells = NULL;
}

void world_clear(world_t *w, char fill) {
  memset(w->cells, fill, (size_t)w->w * (size_t)w->h);
}

void world_add_border(world_t *w, char wall) {
  for (int x = 0; x < (int)w->w; x++) {
    w->cells[idx(w, x, 0)] = wall;
    w->cells[idx(w, x, (int)w->h - 1)] = wall;
  }
  for (int y = 0; y < (int)w->h; y++) {
    w->cells[idx(w, 0, y)] = wall;
    w->cells[idx(w, (int)w->w - 1, y)] = wall;
  }
}

void world_generate_obstacles(world_t *w, float density, unsigned seed) {
  srand(seed);

  // najprv prázdno + border
  world_clear(w, EMPTY);
  world_add_border(w, WALL);

  // vnútro: náhodné steny
  for (int y = 1; y < (int)w->h - 1; y++) {
    for (int x = 1; x < (int)w->w - 1; x++) {
      float r = (float)rand() / (float)RAND_MAX;
      w->cells[idx(w, x, y)] = (r < density) ? WALL : EMPTY;
    }
  }

  w->cells[idx(w, 1, 1)] = EMPTY;
}

int world_is_connected_bfs(const world_t *w) {
  int size = (int)w->w * (int)w->h;
  int *q = (int*)malloc((size_t)size * sizeof(int));
  unsigned char *vis = (unsigned char*)calloc((size_t)size, 1);
  if (!q || !vis) { free(q); free(vis); return 0; }

  int start = -1;
  for (int i = 0; i < size; i++) {
    if (w->cells[i] == EMPTY) { start = i; break; }
  }
  if (start < 0) { free(q); free(vis); return 0; }

  int qh = 0, qt = 0;
  q[qt++] = start;
  vis[start] = 1;

  while (qh < qt) {
    int v = q[qh++];
    int x = v % w->w;
    int y = v / w->w;

    const int dx[4] = {1,-1,0,0};
    const int dy[4] = {0,0,1,-1};
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (nx < 0 || ny < 0 || nx >= (int)w->w || ny >= (int)w->h) continue;
      int ni = idx(w, nx, ny);
      if (!vis[ni] && w->cells[ni] == EMPTY) {
        vis[ni] = 1;
        q[qt++] = ni;
      }
    }
  }

  int ok = 1;
  for (int i = 0; i < size; i++) {
    if (w->cells[i] == EMPTY && !vis[i]) { ok = 0; break; }
  }

  free(q);
  free(vis);
  return ok;
}

int world_place_fruit(world_t *w) {
  // vymaž staré ovocie (ak by bolo)
  int size = (int)w->w * (int)w->h;
  for (int i = 0; i < size; i++) {
    if (w->cells[i] == FRUIT) w->cells[i] = EMPTY;
  }

  int free_count = 0;
  for (int i = 0; i < size; i++) {
    if (w->cells[i] == EMPTY) free_count++;
  }
  if (free_count == 0) return -1;

  int pick = rand() % free_count;
  for (int i = 0; i < size; i++) {
    if (w->cells[i] == EMPTY) {
      if (pick == 0) { w->cells[i] = FRUIT; return 0; }
      pick--;
    }
  }
  return -1;
}

