#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct {
  uint32_t type;
  uint32_t size;
} msg_hdr_t;

enum { MSG_INPUT = 1, MSG_TEXT = 2, MSG_STATE = 3 };

typedef enum { DIR_UP=0, DIR_RIGHT=1, DIR_DOWN=2, DIR_LEFT=3 } dir_t;

typedef struct {
  uint8_t has_dir;
  uint8_t dir;          // dir_t
  uint8_t pause_toggle;
  uint8_t quit;
} msg_input_t;

/* Server -> Client: stav sveta
   Za touto hlavičkou nasleduje payload: w*h bajtov (grid).
*/
typedef struct {
  uint16_t w;
  uint16_t h;
  uint32_t tick;
  uint32_t score;
  uint32_t time_left_ms;
  uint8_t  paused;
  uint8_t  resume_countdown; // 0..3 (zatiaľ 0)
  uint16_t _pad;
} msg_state_hdr_t;

ssize_t write_full(int fd, const void *buf, size_t n);
ssize_t read_full(int fd, void *buf, size_t n);

int send_msg(int fd, uint32_t type, const void *payload, uint32_t size);
int recv_hdr(int fd, msg_hdr_t *h);

#endif

