/*
 * Star of stars style topology, looks like
 * 2           5
 *  \         /
 *   1 - 0 - 4
 *  /         \
 * 3           6
 *
 * Where nodes 2/3 send/recieve, and nodes 0 and 5 send/recieve
*/

#ifndef _TOPOLOGY_H_
#define _TOPOLOGY_H_

#include "config_common.h"
#include "../board_config.h"

#define NUM_NODES 7

static struct srv6_route _routes[] = {
    { .source_id = 2, .dest_id = 3, .segments = "1" },
    { .source_id = 3, .dest_id = 2, .segments = "1" },
    { .source_id = 0, .dest_id = 5, .segments = "4" },
    { .source_id = 5, .dest_id = 0, .segments = "4" }
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
    { .eui_address = EUI_64_ADDR(6), .port = UDP_PORT },
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
    { .role = NODE_ROLE_SENDER, .use_srv6 = USE_SRV6, .dest_id = 5 },
    { .role = NODE_ROLE_FORWARD_ONLY, .use_srv6 = USE_SRV6 },
    { .role = NODE_ROLE_SENDER, .use_srv6 = USE_SRV6, .dest_id = 3 },
    { .role = NODE_ROLE_RECIEVER, .use_srv6 = USE_SRV6, .dest_id = 2 },
    { .role = NODE_ROLE_FORWARD_ONLY, .use_srv6 = USE_SRV6 },
    { .role = NODE_ROLE_RECIEVER, .use_srv6 = USE_SRV6, .dest_id = 0 },
    { .role = NODE_ROLE_FORWARD_ONLY, .use_srv6 = USE_SRV6 },
};

#endif // _TOPOLOGY_H_
