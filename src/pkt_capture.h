#ifndef _PKT_CAPTURE_H_
#define _PKT_CAPTURE_H_

#include "records.h"
#include "src/configs/config_common.h"

struct pkt_capture_thread_args {
    struct record_list *capture_list;
    struct record_list *display_capture_list;
    struct node_configuration *config;
};

void init_pkt_capture_thread(struct pkt_capture_thread_args *args);

#endif // _PKT_CAPTURE_H_
