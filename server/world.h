#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

typedef struct {
  uint16_t w, h;
  char *cells; // w*h znakov
} world_t;

int  world_init(world_t *w, uint16_t width, uint16_t height);
void world_destroy(world_t *w);

void world_clear(world_t *w, char fill);
void world_add_border(world_t *w, char wall);

void world_generate_obstacles(world_t *w, float density, unsigned seed);
int  world_is_connected_bfs(const world_t *w);

int  world_place_fruit(world_t *w); // vráti 0 OK, -1 keď nemá kam dať

#endif
