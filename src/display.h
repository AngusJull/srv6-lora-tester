#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "tsrb.h"

#include "board_config.h"

struct display_thread_args {
    tsrb_t *power_ringbuffer;
    tsrb_t *netstat_ringbuffer;
    tsrb_t *capture_ringbuffer;
    struct node_configuration config;
};

int init_display_thread(struct display_thread_args *args);

#endif // _DISPLAY_H_
