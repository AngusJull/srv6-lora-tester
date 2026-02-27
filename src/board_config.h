#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include "stdint.h"
#include "net/gnrc/netif.h"

// Set the id for this board using compile flags
#ifndef CONFIG_ID
#  define CONFIG_ID 0
#endif

struct address_configuration {
    unsigned int port;    // This node's UDP port
    uint64_t eui_address; // 64 bit EUI, where the bottom 16 bits are a short address
};

struct forwarding_entry {
    unsigned int install_id;  // The node this forwarding entry should be installed on
    unsigned int dest_id;     // The destination for this forwarding entry
    unsigned int next_hop_id; // The next hop towards the destination
};

struct forwarding_configuration {
    struct forwarding_entry *forwarding_entries; // The forwarding entries to add for this node
    unsigned int forwarding_entires_len;         // Length of the forwarding entries array
};

struct srv6_route {
    unsigned int source_id; // Source node id for this route
    unsigned int dest_id;   // Destination node id for this route
    char *segments;         // Segments in this route, not including source or destination
};

struct srv6_configuration {
    struct srv6_route *routes; // All routes for a node
    unsigned int routes_len;   // Length of the routes array
};

enum node_role {
    NODE_ROLE_SENDER,       // Node sends packets
    NODE_ROLE_RECIEVER,     // Node receives and replies to packets
    NODE_ROLE_FORWARD_ONLY, // Node does not listen for packets
};

struct traffic_configuration {
    enum node_role role;  // The role of this node
    int use_srv6;         // If this node should use SRv6
    unsigned int dest_id; // The id to send packets to if this node is a sender
};

struct node_configuration {
    unsigned int this_id;
    struct address_configuration addr_config;          // The addresses of this node and its neighbours
    struct traffic_configuration traffic_config;       // The role for this node and configuration related to send/recieve
    struct srv6_configuration srv6_config;             // The srv6 routes, if this node is a sender and uses srv6 (otherwise null)
    struct forwarding_configuration forwarding_config; // Forwarding entires to add for this node
};

// Get the configured id for this node
unsigned int get_this_id(void);

// Get and apply configuration for a node
struct node_configuration get_node_configuration(unsigned int node_id);
int apply_node_configuration(gnrc_netif_t *netif, struct node_configuration *config);

// Helpers for quickly getting node configuration for a different node
ipv6_addr_t get_node_addr(unsigned int node_id);
unsigned int get_node_port(unsigned int node_id);

#endif // _BOARD_CONFIG_H_
