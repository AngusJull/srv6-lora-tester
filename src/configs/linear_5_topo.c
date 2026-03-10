/*
 * Linear topology with 5 nodes, looks like
 * 0 - 1 - 2 - 3 - 4, where 0 and 4 send/recieve
*/

#include "config_common.h"

#define NUM_NODES 5

static struct srv6_route _routes[] = {
    { .source_id = 0, .dest_id = 4, .segments = "" },
    { .source_id = 4, .dest_id = 0, .segments = "" }
};

static struct forwarding_entry _fib[] = {
    // 0 -> 1 -> 2 -> 3 -> 4
    { .install_id = 0, .dest_id = 4, .next_hop_id = 1 },
    { .install_id = 1, .dest_id = 4, .next_hop_id = 2 },
    { .install_id = 2, .dest_id = 4, .next_hop_id = 3 },
    { .install_id = 3, .dest_id = 4, .next_hop_id = 4 },
    // 4 -> 3 -> 2 -> 1 -> 0
    { .install_id = 4, .dest_id = 0, .next_hop_id = 3 },
    { .install_id = 3, .dest_id = 0, .next_hop_id = 2 },
    { .install_id = 2, .dest_id = 0, .next_hop_id = 1 },
    { .install_id = 1, .dest_id = 0, .next_hop_id = 0 },
};

static struct address_configuration addr_config[NUM_NODES] = {
    { .eui_address = EUI_64_ADDR(0), .port = UDP_PORT },
    { .eui_address = EUI_64_ADDR(1), .port = UDP_PORT },
    { .eui_address = EUI_64_ADDR(2), .port = UDP_PORT },
    { .eui_address = EUI_64_ADDR(3), .port = UDP_PORT },
    { .eui_address = EUI_64_ADDR(4), .port = UDP_PORT },
};

static struct srv6_configuration srv6_config = {
    .routes = _routes,
    .routes_len = LEN(_routes)
};

static struct forwarding_configuration forwarding_config = {
    .forwarding_entries = _fib,
    .forwarding_entires_len = LEN(_fib)
};

static struct traffic_configuration traffic_config[NUM_NODES] = {
    { .role = NODE_ROLE_SENDER, .dest_id = 4 },
    { .role = NODE_ROLE_FORWARD_ONLY },
    { .role = NODE_ROLE_FORWARD_ONLY },
    { .role = NODE_ROLE_FORWARD_ONLY },
    { .role = NODE_ROLE_RECIEVER, .dest_id = 0 },
};

struct topology_configuration linear_5_topo = {
    .addr_configs = addr_config,
    .traffic_configs = traffic_config,
    .srv6_configs = &srv6_config,
    .forwarding_configs = &forwarding_config,
    .num_nodes = NUM_NODES,
};
