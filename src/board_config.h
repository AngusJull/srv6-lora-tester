#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include "net/gnrc/netif.h"
#include "configs/config_common.h"

// Set the id for this board using compile flags
#ifndef CONFIG_ID
#  define CONFIG_ID 0
#endif

#ifndef TOPOLOGY_ID
#  define TOPOLOGY_ID 0
#endif

// Get and apply configuration for a node
struct node_configuration get_node_configuration(void);
int apply_node_configuration(gnrc_netif_t *netif, struct node_configuration *config);

// Helpers for accessing configuration information stored in the topology

static inline struct address_configuration *get_node_addr_config(struct node_configuration *config)
{
    return &config->topology->addr_configs[config->this_id];
}

static inline struct traffic_configuration *get_node_traffic_config(struct node_configuration *config)
{
    return &config->topology->traffic_configs[config->this_id];
}

static inline struct forwarding_configuration *get_topo_forward_config(struct node_configuration *config)
{
    return config->topology->forwarding_configs;
}

static inline struct srv6_configuration *get_topo_srv6_config(struct node_configuration *config)
{
    return config->topology->srv6_configs;
}

// Helpers for quickly getting node configuration for a different node
ipv6_addr_t get_node_addr(unsigned int node_id, struct node_configuration *config);
unsigned int get_node_port(unsigned int node_id, struct node_configuration *config);

#endif // _BOARD_CONFIG_H_
