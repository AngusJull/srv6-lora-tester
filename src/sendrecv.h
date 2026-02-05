#ifndef _SENDRECV_H_
#define _SENDRECV_H_

#include "tsrb.h"
#include "board_config.h"

struct sendrecv_thread_args {
    tsrb_t *latency_tsrb;
    struct node_configuration *config;
};

void init_sendrecv_thread(struct sendrecv_thread_args *args);

#endif // _SENDRECV_H_
