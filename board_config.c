#include "assert.h"

#include "board_config.h"

// Set the id for this board using compile flags
#ifndef THIS_ID
#  define THIS_ID 0
#endif

// Can use compile time flag ot turn on SRv6 usage
#ifndef USE_SRV6
#  define USE_SRV6 0
#endif

#define NUM_NODES  2

#define LEN(array) sizeof(array) / sizeof(array[0])

static struct srv6_route _node0_routes[] = {
    { .dest_id = 1, .segments = "" }
};

static struct srv6_route _node1_routes[] = {
    { .dest_id = 0, .segments = "" }
};

static struct address_configuration addr_config[NUM_NODES] = {
    { .port = 4000, .address = "::0", .neighbours = "1" },
    { .port = 4000, .address = "::1", .neighbours = "0" },
};

static struct srv6_configuration srv6_config[NUM_NODES] = {
    { .routes = _node0_routes, .routes_len = LEN(_node0_routes) },
    { .routes = _node1_routes, .routes_len = LEN(_node1_routes) },
};

static struct traffic_configuration traffic_config[NUM_NODES] = {
    { .role = NODE_ROLE_SENDER, .use_srv6 = USE_SRV6, .dest_id = 1 },
    { .role = NODE_ROLE_RECIEVER, .use_srv6 = USE_SRV6, .dest_id = 0 },
};

unsigned int get_this_id(void)
{
    return THIS_ID;
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

const char *get_node_addr(unsigned int node_id)
{
    assert(node_id < NUM_NODES);
    return addr_config[node_id].address;
}

unsigned int get_node_port(unsigned int node_id)
{
    assert(node_id < NUM_NODES);
    return addr_config[node_id].port;
}
