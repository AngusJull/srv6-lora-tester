#ifndef _STATS_H_
#define _STATS_H_

#include "net/gnrc/netif.h"
#include "tsrb.h"

struct stats_thread_args {
    tsrb_t *power_tsrb;
    tsrb_t *netstat_tsrb;
};

gnrc_netif_t *get_lora_netif(void);
int init_stats_thread(struct stats_thread_args *args);

#endif // _STATS_H_
