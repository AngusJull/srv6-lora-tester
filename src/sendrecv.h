#ifndef _SENDRECV_H_
#define _SENDRECV_H_

#include "records.h"

struct sendrecv_thread_args {
    struct dl_list *latency_list;
    struct node_configuration *config;
};

void init_sendrecv_thread(struct sendrecv_thread_args *args);

#endif // _SENDRECV_H_
