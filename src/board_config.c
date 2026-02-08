#include "assert.h"
#include "net/netif.h"

#include "board_config.h"

#include "current_config.h"

static ipv6_addr_t generate_node_address(struct address_configuration *config)
{
    (void)config;
    ipv6_addr_t address = { 0 };
    return address;
}

static int configure_802154(struct address_configuration *config)
{
    // Set long address
    //
    // Get short address
    // Set short address
    //
    (void)config;
    return 0;
}

static int configure_ipv6(struct address_configuration *config)
{
    // Add address for long address
    //
    // Add address for short address
    (void)config;
    return 0;
}

static int configure_sixlowpan(struct address_configuration *config)
{
    // Add context for prefix
    (void)config;
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
    return config;
}

int apply_node_configuration(netif_t *netif, struct node_configuration *config)
{
    (void)netif;
    (void)config;
    return 0;
}

ipv6_addr_t get_node_addr(unsigned int node_id)
{
    assert(node_id < NUM_NODES);
    return generate_node_address(&addr_config[node_id]);
}

unsigned int get_node_port(unsigned int node_id)
{
    assert(node_id < NUM_NODES);
    return addr_config[node_id].port;
}
