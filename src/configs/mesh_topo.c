/*
 * Sparse mesh topology
 *     3
 *     |
 * 0 - 1 - 2
 * |       |
 * 4 - - - 5
 *
 * Where nodes 0/2 send/recieve and nodes 1/3 send/recieve
 *
 * The SRv6 routes in this file are not calculated using SPF,
 * and show some advantages to using SRv6
*/

#include "config_common.h"

#define NUM_NODES 6

static struct srv6_route _routes[] = {
    { .source_id = 0, .dest_id = 2, .segments = "4" },
    { .source_id = 2, .dest_id = 0, .segments = "4" },
    { .source_id = 1, .dest_id = 3, .segments = "" },
    { .source_id = 3, .dest_id = 1, .segments = "" }
};

static struct forwarding_entry _fib[] = {
    { .install_id = 0, .dest_id = 2, .next_hop_id = 1 },
    { .install_id = 0, .dest_id = 1, .next_hop_id = 1 },
    { .install_id = 0, .dest_id = 4, .next_hop_id = 4 },

    { .install_id = 1, .dest_id = 0, .next_hop_id = 0 },
    { .install_id = 1, .dest_id = 2, .next_hop_id = 2 },
    { .install_id = 1, .dest_id = 3, .next_hop_id = 3 },

    { .install_id = 2, .dest_id = 0, .next_hop_id = 1 },
    { .install_id = 2, .dest_id = 1, .next_hop_id = 1 },
    { .install_id = 2, .dest_id = 5, .next_hop_id = 5 },
    { .install_id = 2, .dest_id = 4, .next_hop_id = 5 },

    { .install_id = 3, .dest_id = 1, .next_hop_id = 1 },

    { .install_id = 4, .dest_id = 0, .next_hop_id = 0 },
    { .install_id = 4, .dest_id = 5, .next_hop_id = 5 },
    { .install_id = 4, .dest_id = 2, .next_hop_id = 5 },

    { .install_id = 5, .dest_id = 2, .next_hop_id = 2 },
    { .install_id = 5, .dest_id = 4, .next_hop_id = 4 },
    { .install_id = 5, .dest_id = 0, .next_hop_id = 4 },
};

static struct address_configuration addr_config[NUM_NODES] = {
    { .eui_address = EUI_64_ADDR(0), .port = UDP_PORT },
    { .eui_address = EUI_64_ADDR(1), .port = UDP_PORT },
    { .eui_address = EUI_64_ADDR(2), .port = UDP_PORT },
    { .eui_address = EUI_64_ADDR(3), .port = UDP_PORT },
    { .eui_address = EUI_64_ADDR(4), .port = UDP_PORT },
    { .eui_address = EUI_64_ADDR(5), .port = UDP_PORT },
};

static struct srv6_configuration srv6_config = {
    .routes = _routes,
    .routes_len = LEN(_routes),
};

static struct forwarding_configuration forwarding_config = {
    .forwarding_entries = _fib,
    .forwarding_entires_len = LEN(_fib)
};

static struct traffic_configuration traffic_config[NUM_NODES] = {
    { .role = NODE_ROLE_SENDER, .dest_id = 2 },
    { .role = NODE_ROLE_RECIEVER, .dest_id = 3 },
    { .role = NODE_ROLE_RECIEVER, .dest_id = 0 },
    { .role = NODE_ROLE_SENDER, .dest_id = 1 },
    { .role = NODE_ROLE_FORWARD_ONLY },
    { .role = NODE_ROLE_FORWARD_ONLY },
};

struct topology_configuration mesh_topo = {
    .addr_configs = addr_config,
    .traffic_configs = traffic_config,
    .srv6_config = &srv6_config,
    .forwarding_config = &forwarding_config,
    .num_nodes = NUM_NODES,
};
