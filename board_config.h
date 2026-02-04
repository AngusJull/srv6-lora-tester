// Configuration for:
//
// L3 addresses, neighbour addresses
// Send/recieve roles, if we use SRv6 or not
// SRv6 routes for sending
//
// Delay time
//

struct l3_configuration {
    char *this_address;
    char **neighbour_addresses;
    unsigned int neighbour_addresses_len;
};

struct srv6_route {
    char *source;
    char **intermediates;
    unsigned int intermediates_len;
    char *sink;
};

struct srv6_configuration {
    struct srv6_route *routes;
    unsigned int num_routes;
};

enum node_role {
    NODE_ROLE_SENDER,
    NODE_ROLE_RECIEVER,
    NODE_ROLE_FORWARD_ONLY,
};

struct traffic_configuration {
    enum node_role role;
    int use_srv6;
};

struct node_configuration {
    struct l3_configuration *l3_config;           /** The addresses of this node and its neighbours */
    struct srv6_configuration *srv6_config;       /** The srv6 routes, if this node is a sender and uses srv6 (otherwise null) */
    struct traffic_configuration *traffic_config; /** The role for this node and configuration related to send/recieve */
};
