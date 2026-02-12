#include "assert.h"
#include "net/gnrc/ipv6/nib/ft.h"
#include "net/gnrc/netif/conf.h"
#include "net/gnrc/sixlowpan/ctx.h"

#include "current_config.h"
#include "net/ipv6/addr.h"
#include "net/netopt.h"
#include "board_config.h"

#define ENABLE_DEBUG 0
#include "debug.h"

static uint64_t short_addr_iid(uint64_t eui_address)
{
    uint16_t short_addr = EUI_64_TO_SHORT_ADDR(eui_address);
    return SHORT_ADDR_TO_IID(short_addr);
}

static ipv6_addr_t generate_ipv6_addr(uint64_t iid)
{
    ipv6_addr_t prefix;
    if (ipv6_addr_from_str(&prefix, NETWORK_PREFIX) == NULL) {
        printf("Could not load prefix\n");
    }

    ipv6_addr_t addr;
    ipv6_addr_set_iid(&addr, iid);
    ipv6_addr_init_prefix(&addr, &prefix, NETWORK_PREFIX_LEN);

    return addr;
}

// Configure the IEEE 802.15.4 addresses for the radio provided
// Configure both long and short addresses to make sure things stay consistent
static int configure_802154(gnrc_netif_t *netif, struct address_configuration *config)
{
    uint16_t pan_id = PAN_ID;
    if (gnrc_netapi_set(netif->pid, NETOPT_NID, 0, &pan_id, sizeof(pan_id)) < 0) {
        printf("Failed to configure PAN ID of %u\n", pan_id);
        return -1;
    }
    DEBUG("Configured PAN ID %u\n", pan_id);

    // Use union to allow printing of u64 as two u32s, because printing u64s isn't supported by RIOT without additional module
    union {
        uint64_t u64;
        uint32_t u32[2];
    } long_addr;
    long_addr.u64 = config->eui_address;
    uint64_t long_addr_ordered = htonll(long_addr.u64);
    if (gnrc_netapi_set(netif->pid, NETOPT_ADDRESS_LONG, 0, &long_addr_ordered, sizeof(long_addr_ordered)) < 0) {
        printf("Failed to configure long addr of 0x%" PRIx32 "%" PRIx32 "\n", long_addr.u32[0], long_addr.u32[1]);
        return -1;
    }
    DEBUG("Configured long address of 0x%" PRIx32 "%" PRIx32 "\n", long_addr.u32[1], long_addr.u32[0]);

    // Set both the short and long addresses so there isn't any mismatch with what we configure for IPv6
    uint16_t short_addr = EUI_64_TO_SHORT_ADDR(config->eui_address);
    uint16_t short_addr_ordered = htons(short_addr);
    if (gnrc_netapi_set(netif->pid, NETOPT_ADDRESS, 0, &short_addr_ordered, sizeof(short_addr_ordered)) < 0) {
        printf("Failed to configure short addr of 0x%x\n", short_addr);
        return -1;
    }
    DEBUG("Configured short address 0x%x\n", short_addr);

    return 0;
}

static void remove_all_ipv6_addrs(gnrc_netif_t *netif)
{
    ipv6_addr_t curr_addrs[CONFIG_GNRC_NETIF_IPV6_ADDRS_NUMOF];
    int addrs_len = gnrc_netif_ipv6_addrs_get(netif, curr_addrs, sizeof(curr_addrs));
    for (unsigned i = 0; i < addrs_len / sizeof(curr_addrs[0]); i++) {
        // Remove the site local address, since it is link local and includes old hw address
        gnrc_netif_ipv6_addr_remove(netif, &curr_addrs[i]);
#if ENABLE_DEBUG == 1
        DEBUG("Removing address ");
        ipv6_addr_print(&curr_addrs[i]);
        DEBUG("\n");
#endif
    }
}

static int configure_ipv6(gnrc_netif_t *netif, struct address_configuration *config)
{
    // We need to remove previous IP addresses to make sure all our messages are being compressed properly
    remove_all_ipv6_addrs(netif);

    ipv6_addr_t long_addr = generate_ipv6_addr(config->eui_address);
    if (gnrc_netif_ipv6_addr_add(netif, &long_addr, NETWORK_PREFIX_LEN, 0) < 0) {
        printf("Could not set IPv6 address for long address\n");
        ipv6_addr_print(&long_addr);
        return -1;
    }

    // We add both long and short addresses so we can choose between using either in messages
    ipv6_addr_t short_addr = generate_ipv6_addr(short_addr_iid(config->eui_address));
    if (gnrc_netif_ipv6_addr_add(netif, &short_addr, NETWORK_PREFIX_LEN, 0) < 0) {
        printf("Could not set IPv6 address for short address\n");
        ipv6_addr_print(&short_addr);
        return -1;
    }

#if ENABLE_DEBUG == 1
    // Using conditional block to use built in prints instead of messing with big endian u64s
    DEBUG("Configured short address: ");
    ipv6_addr_print(&short_addr);
    DEBUG("\nConfigured long address: ");
    ipv6_addr_print(&long_addr);
    DEBUG("\n");
#endif

    return 0;
}

static int configure_sixlowpan(void)
{
    ipv6_addr_t prefix;
    if (ipv6_addr_from_str(&prefix, NETWORK_PREFIX) == NULL) {
        printf("Could not load routing prefix\n");
        return -1;
    }

    // Setting lifetime to 0 prevents it from being used. So set lifetime so high its never invalid
    // Only need to add a context for the prefix we've used in IPv6 addresses
    if (!gnrc_sixlowpan_ctx_update_6ctx(&prefix, NETWORK_PREFIX_LEN, UINT32_MAX)) {
        printf("Could not add compression context\n");
        return -1;
    }

    DEBUG("Added compression context from prefix %s\n", NETWORK_PREFIX);
    return 0;
}

static int configure_forwarding_entries(gnrc_netif_t *netif, struct forwarding_configuration *config)
{
    for (unsigned i = 0; i < config->forwarding_entires_len; i++) {
        struct forwarding_entry *entry = &config->forwarding_entries[i];
        ipv6_addr_t dest_addr = get_node_addr(entry->dest_id);
        ipv6_addr_t next_hop_addr = get_node_addr(entry->next_hop_id);
        // Currently, only add fully specified next hops and destinations, to keep things simple
        if (gnrc_ipv6_nib_ft_add(&dest_addr, IPV6_ADDR_BIT_LEN, &next_hop_addr, netif->pid, 0) != 0) {
            printf("Failed to add a routing table entry for dest %u, next hop %u\n", entry->dest_id, entry->next_hop_id);
            return -1;
        }
        DEBUG("Added a routing table entry for dest %u, next hop %u\n", entry->dest_id, entry->next_hop_id);
    }
    return 0;
}

unsigned int get_this_id(void)
{
    return CONFIG_ID;
}

struct node_configuration get_node_configuration(unsigned int node_id)
{
    // Should never ask for an invalid node
    assert(node_id < NUM_NODES);
    // Make copies in case we need to get these later
    struct node_configuration config;
    config.this_id = node_id;
    config.addr_config = addr_config[node_id];
    config.srv6_config = srv6_config[node_id];
    config.traffic_config = traffic_config[node_id];
    config.forwarding_config = forwarding_config[node_id];
    return config;
}

int apply_node_configuration(gnrc_netif_t *netif, struct node_configuration *config)
{
    if (configure_802154(netif, &config->addr_config) < 0) {
        puts("Could not apply 802.15.4 configuration completely\n");
        return -1;
    }
    if (configure_ipv6(netif, &config->addr_config) < 0) {
        puts("Could not apply IPv6 conifguration completely\n");
        return -1;
    }
    if (configure_sixlowpan()) {
        puts("Could not apply sixlowpan confiugration completely\n");
        return -1;
    }
    if (configure_forwarding_entries(netif, &config->forwarding_config)) {
        puts("Could not apply forwarding confiugration completely\n");
        return -1;
    }
    return 0;
}

ipv6_addr_t get_node_addr(unsigned int node_id)
{
    assert(node_id < NUM_NODES);
    // Use the long address to create the IPv6 address (could try short address instead later)
    return generate_ipv6_addr(addr_config[node_id].eui_address);
}

unsigned int get_node_port(unsigned int node_id)
{
    assert(node_id < NUM_NODES);
    return addr_config[node_id].port;
}
