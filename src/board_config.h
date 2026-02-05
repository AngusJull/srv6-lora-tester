#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

struct address_configuration {
    unsigned int port; // This node's UDP port
    char *address;     // This nodes IPv6 address
    char *neighbours;  // List of neighbours. Currently in node ids, but could switch to IPv6 addresses
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
const char *get_node_addr(unsigned int node_id);
unsigned int get_node_port(unsigned int node_id);

#endif // _BOARD_CONFIG_H_
