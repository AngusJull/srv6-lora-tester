#include "assert.h"
#include "net/gnrc/ipv6/nib/ft.h"
#include "net/gnrc/netif/conf.h"
#include "net/gnrc/sixlowpan/ctx.h"
#include "net/ipv6/addr.h"
#include "net/netopt.h"
#include "periph/flashpage.h"
#include "checksum/crc8.h"

#include "board_config.h"
#include "configs/topology.h"

#define ENABLE_DEBUG 0
#include "debug.h"

// Use page 1 instead of 0 because otherwise a flashpage macro causes clang to warn about out of bounds access
#define FLASHPAGE                1

// Use CRC parameters based on some common values. Anything should work
#define CRC_POLY                 0x07
#define CRC_INIT                 0x00

// Flip the flag to keep the same scope (locally assigned stays locally assigned)
#define FLIP_EUI_LOCAL_FLAG(eui) ((eui) ^ 0x200000000000000)

// The format to save configuration information in flashpage writes must be in multiples of 4 bytes on the esp32, so align to 4 bytes
// Include a crc to check for bad data
struct __aligned(4) saved_config_stored {
    struct saved_config data;
    uint8_t crc8; // CRC for detecting if data is corrupt or missing
};

// Generate a global IPv6 address for a node based on its hardware address
static ipv6_addr_t generate_ipv6_addr(uint64_t eui)
{
    ipv6_addr_t addr;
    if (ipv6_addr_from_str(&addr, NETWORK_PREFIX) == NULL) {
        printf("Could not load prefix\n");
    }

    eui = FLIP_EUI_LOCAL_FLAG(eui);
    ipv6_addr_set_iid(&addr, eui);
    return addr;
}

// Generate a link local IPv6 address for a node based on its hardware address
static ipv6_addr_t generate_link_local_ipv6_addr(uint64_t eui)
{
    ipv6_addr_t addr;
    ipv6_addr_set_link_local_prefix(&addr);

    eui = FLIP_EUI_LOCAL_FLAG(eui);
    ipv6_addr_set_iid(&addr, eui);
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

// Remove all existing IPv6 unicast addresses from an interface (to prevent any traffic from taking an unintended route)
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

// Perform all configuration related to the IPv6 stack
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

    ipv6_addr_t local_addr = generate_link_local_ipv6_addr(config->eui_address);
    if (gnrc_netif_ipv6_addr_add(netif, &local_addr, NETWORK_PREFIX_LEN, 0) < 0) {
        printf("Could not set link local IPv6 address\n");
        ipv6_addr_print(&long_addr);
        return -1;
    }

    // Using conditional block to use built in prints instead of messing with big endian u64s
#if ENABLE_DEBUG == 1
    DEBUG("Configured link local address: ");
    ipv6_addr_print(&local_addr);
    DEBUG("\nConfigured long address: ");
    ipv6_addr_print(&long_addr);
    DEBUG("\n");
#endif

    return 0;
}

// Perform configuration related to the sixlowpan stack
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

// Perform configuration related to forwarding on this node - done statically to ensure our routes for data are used
static int configure_forwarding_entries(gnrc_netif_t *netif, struct node_configuration *config)
{
    for (unsigned i = 0; i < get_topo_forward_config(config)->forwarding_entires_len; i++) {
        struct forwarding_entry *entry = &get_topo_forward_config(config)->forwarding_entries[i];
        if (entry->install_id == config->this_id) {
            ipv6_addr_t dest_addr = get_node_addr(entry->dest_id, config);
            assert(entry->dest_id < config->topology->num_nodes);
            ipv6_addr_t next_hop_addr = generate_link_local_ipv6_addr(config->topology->addr_configs[entry->next_hop_id].eui_address);
            // Currently, only add fully specified next hops and destinations, to keep things simple
            if (gnrc_ipv6_nib_ft_add(&dest_addr, IPV6_ADDR_BIT_LEN, &next_hop_addr, netif->pid, 0) != 0) {
                printf("Failed to add a routing table entry for dest %u, next hop %u\n", entry->dest_id, entry->next_hop_id);
                return -1;
            }
            DEBUG("Added a routing table entry for dest %u, next hop %u\n", entry->dest_id, entry->next_hop_id);
        }
    }
    return 0;
}

static struct saved_config default_saved_config(void)
{
    return (struct saved_config){ .config_id = CONFIG_ID, .chosen_topology = TOPOLOGY_ID, .use_srv6 = USE_SRV6, .throughput_test = 0 };
}

// Get configuration information related to a node (must be a valid node)
struct node_configuration get_node_configuration(void)
{
    struct saved_config loaded_config = get_saved_configuration();
    struct node_configuration node_config;

    assert(loaded_config.chosen_topology < TOPOLOGY_NUM_TOPOLOGIES);
    node_config.topology = topology_array[loaded_config.chosen_topology];
    node_config.topology_id = loaded_config.chosen_topology;

    if (loaded_config.config_id >= node_config.topology->num_nodes) {
        DEBUG("Config ID (%d) does not match topology. Reverting to default ID\n", loaded_config.config_id);
        loaded_config.config_id = 0;
    }
    node_config.this_id = loaded_config.config_id;

    node_config.use_srv6 = loaded_config.use_srv6;
    node_config.throughput_test = loaded_config.throughput_test;

    return node_config;
}

// Apply all of a node's configuration (should be done first, before any applications run)
int apply_node_configuration(gnrc_netif_t *netif, struct node_configuration *config)
{
    if (configure_802154(netif, get_node_addr_config(config)) < 0) {
        printf("Could not apply 802.15.4 configuration completely\n");
        return -1;
    }
    if (configure_ipv6(netif, get_node_addr_config(config)) < 0) {
        printf("Could not apply IPv6 conifguration completely\n");
        return -1;
    }
    if (configure_sixlowpan()) {
        printf("Could not apply sixlowpan confiugration completely\n");
        return -1;
    }
    if (configure_forwarding_entries(netif, config)) {
        printf("Could not apply forwarding confiugration completely\n");
        return -1;
    }
    return 0;
}

// Get the global address that should be used to refer to a node when sending traffic
ipv6_addr_t get_node_addr(unsigned int node_id, struct node_configuration *config)
{
    assert(node_id < config->topology->num_nodes);
    // Use the long address to create the IPv6 address (could try short address instead later)
    return generate_ipv6_addr(config->topology->addr_configs[node_id].eui_address);
}

// Get the id of a node with a certain address, or -1 if its not a valid address for a node
int get_node_id(ipv6_addr_t *addr, struct node_configuration *config)
{
    // Get eui, which should uniquely identify a node
    uint64_t iid = byteorder_ntohll(addr->u64[1]);
    uint64_t eui = FLIP_EUI_LOCAL_FLAG(iid);
    // Using an XOR, we can remove all the EUI prefix bits, assuming a node id of zero does not set any bits itself
    uint64_t node_id = EUI_64_ADDR(0) ^ eui;

    // Nodes not using the correct EUI prefix, or which have an ID too high for this topology should be ignored
    if (node_id >= config->topology->num_nodes) {
        DEBUG("Could not turn address back into node id");
        return -1;
    }
    return (int)node_id;
}

// Get the port a node is reachable on
unsigned int get_node_port(unsigned int node_id, struct node_configuration *config)
{
    assert(node_id < config->topology->num_nodes);
    return config->topology->addr_configs[node_id].port;
}

struct srv6_route *get_srv6_route(unsigned int source_id, unsigned int dest_id, struct node_configuration *config)
{
    struct srv6_configuration *srv6_config = config->topology->srv6_config;
    for (unsigned int i = 0; i < srv6_config->routes_len; i++) {
        if (srv6_config->routes[i].source_id == source_id && srv6_config->routes[i].dest_id == dest_id) {
            return &srv6_config->routes[i];
        }
    }
    return NULL;
}

struct saved_config get_saved_configuration(void)
{
    // The format for the struct and the flashpage array _must_ match
    // For some reason it seems using a memcpy here causes the value of fields to invert upon returning. Very confusing
    struct saved_config_stored stored_config = *(struct saved_config_stored *)flashpage_addr(FLASHPAGE);
    unsigned int crc = crc8((uint8_t *)&stored_config.data, sizeof(stored_config.data), CRC_POLY, CRC_INIT);

    struct saved_config config = stored_config.data;
    if (crc != stored_config.crc8) {
        DEBUG("Loaded config is invalid, providing default\n");
        config = default_saved_config();
        set_saved_configuration(config);
    }
    DEBUG("Loaded configuration with fields config id %u, topology %u, use_srv6 %u, throughput test %u\n",
          config.config_id, config.chosen_topology, config.use_srv6, config.throughput_test);
    return config;
}

void set_saved_configuration(struct saved_config config)
{
    struct saved_config_stored stored_config = { .data = config };
    stored_config.crc8 = crc8((uint8_t *)&stored_config.data, sizeof(stored_config.data), CRC_POLY, CRC_INIT);

    flashpage_erase(FLASHPAGE);
    flashpage_write(flashpage_addr(FLASHPAGE), &stored_config, sizeof(stored_config));
    DEBUG("Wrote configuration with fields config id %u, topology %u, use_srv6 %u, throughput test %u\n", config.config_id, config.chosen_topology, config.use_srv6, config.throughput_test);
}
