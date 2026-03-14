#include "net/netif.h"
#include "ztimer.h"

#include "battery.h"
#include "records.h"
#include "stats.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#define TIME_BETWEEN_STATS_COLLECTION_MS 5000
#define NUM_BATTERY_SAMPLES              10
#define BATTERY_SAMPLE_SLEEP_MS          10

#define STATS_THREAD_PRIORITY            (THREAD_PRIORITY_MAIN - 2)
#define THREAD_PRIORITY_MED              (THREAD_PRIORITY_MAIN + 1)
// Stack for the stats thread
static char _stack[THREAD_STACKSIZE_MEDIUM];

static void print_netif_name(netif_t *netif)
{
    static char name[CONFIG_NETIF_NAMELENMAX] = "None";
    if (netif != NULL) {
        netif_get_name(netif, name);
    }
    printf("Radio name: %s\n", name);
}

static void copy_netstat_to_record(netstats_t *from, struct netstats *dest)
{
    dest->rx_bytes = from->rx_bytes;
    dest->tx_bytes = from->tx_bytes;
    dest->tx_unicast_count = from->tx_unicast_count;
    dest->rx_count = from->rx_count;
}

static int collect_netstats(netif_t *netif, unsigned int type, struct netstats *dest)
{
    netstats_t stats;
    if (netif_get_opt(netif, NETOPT_STATS, type, &stats, sizeof(stats)) < 0) {
        DEBUG("Could not read stats\n");
    }
    else {
        copy_netstat_to_record(&stats, dest);
    }
    return 0;
}

static unsigned int avg_battery(void)
{
    unsigned int sum = 0;
    for (unsigned int i = 0; i < NUM_BATTERY_SAMPLES; i++) {
        sum += read_battery_mv();
        ztimer_sleep(ZTIMER_MSEC, BATTERY_SAMPLE_SLEEP_MS);
    }
    return sum / NUM_BATTERY_SAMPLES;
}

// Stats collection and recording loop
static void *_stats_loop(void *ctx)
{
    struct stats_thread_args *args = ctx;
    gnrc_netif_t *radio_netif = get_lora_netif();
    DEBUG("Radio netif identified as %p\n", radio_netif);
    print_netif_name(&radio_netif->netif);

    init_battery_adc();
    while (1) {
        struct stats_record record = { 0 };
        // Get the avg battery first because sampling takes some time
        record.millivolts = avg_battery();
        // Now can set the time
        record.time = ztimer_now(ZTIMER_MSEC);
        // If we don't have a netif, just leave stats zeroed. Should be fine
        if (radio_netif) {
            // Collect the L2 and IPv6 stats independently so we can see difference between the two
            collect_netstats(&radio_netif->netif, NETSTATS_LAYER2, &record.l2);
            collect_netstats(&radio_netif->netif, NETSTATS_IPV6, &record.l3);
        }
        record_list_insert(args->stats_list, (uint8_t *)&record, sizeof(record));

        DEBUG("Stats sleeping\n");
        ztimer_sleep(ZTIMER_MSEC, TIME_BETWEEN_STATS_COLLECTION_MS);
    }
    return NULL;
}

gnrc_netif_t *get_lora_netif(void)
{
    return gnrc_netif_get_by_type(NETDEV_SX126X, NETDEV_INDEX_ANY);
}

// Start collecting stats
int init_stats_thread(struct stats_thread_args *args)
{
    thread_create(_stack, sizeof(_stack), STATS_THREAD_PRIORITY, 0, _stats_loop, args, "stats collection");
    return 0;
}
