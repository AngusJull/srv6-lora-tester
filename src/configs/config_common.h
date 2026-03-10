/* Common information between different types of configurations */

#ifndef _CONFIG_COMMON_H_
#define _CONFIG_COMMON_H_

#include "stdint.h"

// Can use compile time flag ot turn on SRv6 usage
#ifndef USE_SRV6
#  define USE_SRV6 0
#endif

// PAN ID for the network to use
#ifndef PAN_ID
#  define PAN_ID 0xABCD
#endif

// Context number for 6LoWPAN compression. Use 1 in case RIOT has a default use for 0
#ifndef CONTEXT_ID
#  define CONTEXT_ID 1
#endif

// EUI prefix, so that node short addresses can just be the last two bytes. Pattern should help spot problems
// Keep bit 7 as 1 (locally assigned)
#define EUI_PREFIX_48                 0xAEFEBEFECEFE0000

// Generate fake EUI 64 addresses so we can statically configure the network
#define EUI_64_ADDR(node_id)          (EUI_PREFIX_48 | 0x1000 | (node_id))

// Keep the bottom byte when generating a short address
#define EUI_64_TO_SHORT_ADDR(address) (address & 0xFFFF)

// Turn a short address into an IID to use in an IPv6 address
// An IID for a short addr is of the form "::00ff:fe00:XXXX"
#define SHORT_ADDR_TO_IID(address)    (0x00FFFE000000 | address)

// Prefix for locally assigned addresses. RIOT has a different prefix, which is outdated as of 2004
// (see Wikipedia for Unique Local Addresses)
#define ULA_PREFIX                    "fd"

// Network prefix, minus the subnet. Taken from the Wikipedia page for ULAs
#ifndef ROUTING_PREFIX
#  define ROUTING_PREFIX ULA_PREFIX "ab:0:0"
#endif

// Subnet
#ifndef SUBNET
#  define SUBNET "0001"
#endif

// Network prefix, which preceeds all addresses. /64 length
#ifndef NETWORK_PREFIX
#  define NETWORK_PREFIX ROUTING_PREFIX ":" SUBNET "::"
#endif

#ifndef NETWORK_PREFIX_LEN
#  define NETWORK_PREFIX_LEN 64
#endif

#ifndef UDP_PORT
#  define UDP_PORT 4000
#endif

// Helper when defining configuration arrays
#define LEN(array) sizeof(array) / sizeof(array[0])

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
    unsigned int dest_id; // The id to send packets to if this node is a sender
};

struct topology_configuration {
    struct srv6_configuration *srv6_config;             // SRv6 configuration for certain nodes
    struct forwarding_configuration *forwarding_config; // Forwarding configuration for certain nodes
    struct address_configuration *addr_configs;         // Array of length num_nodes, address configuration for each node for this topology
    struct traffic_configuration *traffic_configs;      // Array of length num_nodes, traffic configuration for each node for this topology
    unsigned int num_nodes;
};

struct node_configuration {
    unsigned int this_id;
    unsigned int use_srv6;
    struct topology_configuration *topology;
};

#endif // _CONFIG_COMMON_H_
