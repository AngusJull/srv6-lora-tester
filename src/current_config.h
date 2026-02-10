/**
 * This file contains the details about a specific board configuration that is to be compiled into the
 * application
 */

#ifndef _CURRENT_CONFIG_H_
#define _CURRENT_CONFIG_H_

#include "board_config.h"

#define NUM_NODES            2

#define LEN(array)           sizeof(array) / sizeof(array[0])

// EUI prefix, so that node short addresses can just be the last two bytes. Patten should help spot problems
#define EUI_PREFIX_48        0xAEFEBEFECEFE0000

#define UDP_PORT             4000

// Generate fake EUI 64 addresses so we can statically configure the network
#define EUI_64_ADDR(node_id) EUI_PREFIX_48 | 0x1000 | node_id

static struct srv6_route _node0_routes[] = {
    { .dest_id = 1, .segments = "" }
};

static struct srv6_route _node1_routes[] = {
    { .dest_id = 0, .segments = "" }
};

static struct address_configuration addr_config[NUM_NODES] = {
    { .eui_address = EUI_64_ADDR(0), .port = UDP_PORT, .neighbours = "1" },
    { .eui_address = EUI_64_ADDR(1), .port = UDP_PORT, .neighbours = "0" },
};

static struct srv6_configuration srv6_config[NUM_NODES] = {
    { .routes = _node0_routes, .routes_len = LEN(_node0_routes) },
    { .routes = _node1_routes, .routes_len = LEN(_node1_routes) },
};

static struct traffic_configuration traffic_config[NUM_NODES] = {
    { .role = NODE_ROLE_SENDER, .use_srv6 = USE_SRV6, .dest_id = 1 },
    { .role = NODE_ROLE_RECIEVER, .use_srv6 = USE_SRV6, .dest_id = 0 },
};

#endif // _CURRENT_CONFIG_H_
