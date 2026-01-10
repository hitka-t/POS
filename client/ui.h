#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <common/protocol.h>

void ui_init(void);
void ui_shutdown(void);

// blokuje kým nepríde kláves
// vráti msg_input_t podľa šípok/p/q
msg_input_t ui_read_input(void);

// zobrazí text zo servera
void ui_show_status(const char *text);

void ui_draw_world(uint16_t w, uint16_t h, const char *grid,
                   uint32_t score, uint32_t tick,
                   uint8_t paused, uint8_t resume_countdown);

#endif
