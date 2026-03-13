#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "records.h"

struct display_thread_args {
    struct record_list *power_list;
    struct record_list *netstat_list;
    struct record_list *capture_list;
    struct node_configuration *config;
};

int init_display_thread(struct display_thread_args *args);

#endif // _DISPLAY_H_
