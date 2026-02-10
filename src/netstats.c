#include "net/netdev.h"
#include "net/netopt.h"
#include "netstats.h"

#define ENABLE_DEBUG 0
#include "debug.h"

gnrc_netif_t *get_lora_netif(void)
{
    return gnrc_netif_get_by_type(NETDEV_SX126X, NETDEV_INDEX_ANY);
}

int get_stats(netif_t *netif, unsigned int type, netstats_t *stats)
{
    return netif_get_opt(netif, NETOPT_STATS, type, stats, sizeof(*stats));
}

void print_netif_name(netif_t *netif)
{
    static char name[CONFIG_NETIF_NAMELENMAX] = "None";
    if (netif != NULL) {
        netif_get_name(netif, name);
    }
    printf("Radio name: %s\n", name);
}
