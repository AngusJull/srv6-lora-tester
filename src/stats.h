#ifndef _STATS_H_
#define _STATS_H_

#include "net/gnrc/netif.h"

#include "records.h"

struct stats_thread_args {
    struct dl_list *power_list;
    struct dl_list *netstat_list;
};

gnrc_netif_t *get_lora_netif(void);
int init_stats_thread(struct stats_thread_args *args);

#endif // _STATS_H_
