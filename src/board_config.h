#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include "stdint.h"
#include "net/gnrc/netif.h"

// Set the id for this board using compile flags
#ifndef CONFIG_ID
#  define CONFIG_ID 0
#endif

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

struct address_configuration {
    unsigned int port;    // This node's UDP port
    uint64_t eui_address; // 64 bit EUI, where the bottom 16 bits are a short address
    char *neighbours;     // List of neighbours. Currently in node ids, but could switch to IPv6 addresses
};

struct srv6_route {
    unsigned int dest_id; // Destination node id for this route
    char *segments;       // Segments in this route, not including source or destination
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
    struct address_configuration addr_config;    // The addresses of this node and its neighbours
    struct srv6_configuration srv6_config;       // The srv6 routes, if this node is a sender and uses srv6 (otherwise null)
    struct traffic_configuration traffic_config; // The role for this node and configuration related to send/recieve
};

unsigned int get_this_id(void);
struct node_configuration get_node_configuration(unsigned int node_id);
int apply_node_configuration(gnrc_netif_t *netif, struct node_configuration *config);
ipv6_addr_t get_node_addr(unsigned int node_id);
unsigned int get_node_port(unsigned int node_id);

#endif // _BOARD_CONFIG_H_
