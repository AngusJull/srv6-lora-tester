/*
 * Sparse mesh topology
 *     3
 *     |
 * 0 - 1 - 2
 * |       |
 * 4 - - - 5
 *
 * Where nodes 0/2 send/recieve and nodes 1/3 send/recieve
*/

#ifndef _TOPOLOGY_H_
#define _TOPOLOGY_H_

#include "config_common.h"
#include "../board_config.h"

#define NUM_NODES 6

static struct srv6_route _routes[] = {
    { .source_id = 0, .dest_id = 2, .segments = "4 5" },
    { .source_id = 2, .dest_id = 0, .segments = "5 4" },
    { .source_id = 1, .dest_id = 3, .segments = "" },
    { .source_id = 3, .dest_id = 1, .segments = "" }
};

static struct forwarding_entry _fib[] = {
    { .install_id = 2, .dest_id = 3, .next_hop_id = 1 },
    { .install_id = 2, .dest_id = 1, .next_hop_id = 1 },
    { .install_id = 1, .dest_id = 3, .next_hop_id = 3 },
    { .install_id = 1, .dest_id = 2, .next_hop_id = 2 },
    { .install_id = 0, .dest_id = 5, .next_hop_id = 4 },
    { .install_id = 0, .dest_id = 4, .next_hop_id = 4 },
    { .install_id = 4, .dest_id = 5, .next_hop_id = 5 },
    { .install_id = 4, .dest_id = 0, .next_hop_id = 0 },
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
    { .role = NODE_ROLE_SENDER, .use_srv6 = USE_SRV6, .dest_id = 2 },
    { .role = NODE_ROLE_SENDER, .use_srv6 = USE_SRV6, .dest_id = 3 },
    { .role = NODE_ROLE_RECIEVER, .use_srv6 = USE_SRV6, .dest_id = 0 },
    { .role = NODE_ROLE_RECIEVER, .use_srv6 = USE_SRV6, .dest_id = 1 },
    { .role = NODE_ROLE_FORWARD_ONLY, .use_srv6 = USE_SRV6 },
    { .role = NODE_ROLE_FORWARD_ONLY, .use_srv6 = USE_SRV6 },
};

#endif // _TOPOLOGY_H_
