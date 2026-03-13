#ifndef _PKT_CAPTURE_H_
#define _PKT_CAPTURE_H_

#include "records.h"

struct pkt_capture_thread_args {
    struct record_list *capture_list;
};

void init_pkt_capture_thread(struct pkt_capture_thread_args *args);

#endif // _PKT_CAPTURE_H_
