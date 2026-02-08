#include "net/netif.h"
#include "net/netstats.h"
#include "netstats.h"
#include "battery.h"
#include "ztimer.h"

#include "stats.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#define S_TO_MS               1000

#define STATS_THREAD_PRIORITY (THREAD_PRIORITY_MAIN - 2)
#define THREAD_PRIORITY_MED   (THREAD_PRIORITY_MAIN + 1)
// Stack for the stats thread
static char _stack[THREAD_STACKSIZE_MEDIUM];

int add_record(tsrb_t *tsrb, uint8_t *record, size_t size)
{
    // Overwrite oldest value
    if (tsrb_free(tsrb) < size) {
        // Must drop whole records so we don't corrupt data
        tsrb_drop(tsrb, size);
        // Assume this completed successfully, but if it doesn't should just be unable to add
    }
    return tsrb_add(tsrb, record, size);
}

static void copy_netstat_to_record(netstats_t *from, enum netstat_type type, struct netstat_record *dest)
{
    dest->rx_bytes = from->rx_bytes;
    dest->tx_bytes = from->tx_bytes;
    dest->tx_failed = from->tx_failed;
    dest->tx_success = from->tx_success;
    dest->tx_mcast_count = from->tx_mcast_count;
    dest->tx_unicast_count = from->tx_unicast_count;
    dest->rx_count = from->rx_count;
    dest->type = type;
    // Must use the same clock as other uses of ztimer_now
    dest->time = ztimer_now(ZTIMER_MSEC);
}

static int collect_netstats(netif_t *netif, unsigned int type, tsrb_t *dest)
{
    netstats_t stats;
    if (get_stats(netif, type, &stats) < 0) {
        DEBUG("Could not read stats\n");
    }
    else {
        struct netstat_record netstats;
        enum netstat_type record_type;
        switch (type) {
        case NETSTATS_ALL:
            record_type = NETSTATS_RECORD_TYPE_ALL;
            break;
        case NETSTATS_IPV6:
            record_type = NETSTATS_RECORD_TYPE_IPV6;
            break;
        case NETSTATS_LAYER2:
            record_type = NETSTATS_RECORD_TYPE_L2;
            break;
        default:
            return -1;
        }
        copy_netstat_to_record(&stats, record_type, &netstats);
        add_record(dest, (uint8_t *)&netstats, sizeof(netstats));
    }
    return 0;
}

// Stats collection and recording loop
static void *_stats_loop(void *ctx)
{
    struct stats_thread_args *args = ctx;
    netif_t *radio_netif = get_lora_netif();
    DEBUG("Radio netif identified as %p\n", radio_netif);
    print_netif_name(radio_netif);

    init_battery_adc();
    while (1) {
        // Get new information
        struct power_record power = { .time = ztimer_now(ZTIMER_MSEC), .millivolts = read_battery_mv() };
        add_record(args->power_tsrb, (uint8_t *)&power, sizeof(power));

        if (radio_netif) {
            // Collect the L2 and IPv6 stats independently so we can see difference between the two
            collect_netstats(radio_netif, NETSTATS_LAYER2, args->netstat_tsrb);
            collect_netstats(radio_netif, NETSTATS_IPV6, args->netstat_tsrb);
        }
        DEBUG("Stats sleeping\n");
        ztimer_sleep(ZTIMER_MSEC, S_TO_MS);
    }
    return NULL;
}

// Start collecting stats
int init_stats_thread(struct stats_thread_args *args)
{
    thread_create(_stack, sizeof(_stack), STATS_THREAD_PRIORITY, 0, _stats_loop, args, "stats collection");
    return 0;
}
