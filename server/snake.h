#ifndef SNAKE_H
#define SNAKE_H

#include <stdint.h>
#include "world.h"
#include <common/protocol.h>

typedef struct {
  int x, y;
} pos_t;

typedef struct {
  pos_t *body;       // [0] je hlava
  int capacity;
  int len;
  dir_t dir;
  int grow;          // koľko segmentov ešte má narásť
} snake_t;

int  snake_init(snake_t *s, int max_cells);
void snake_destroy(snake_t *s);

void snake_spawn(snake_t *s, const world_t *w);  // nájde voľné miesto
int  snake_step(snake_t *s, world_t *w, uint32_t *score, int world_type); // 0 ok, -1 game over

void snake_set_dir(snake_t *s, dir_t d);

#endif
