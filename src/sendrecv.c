// Create a  thread with the same priority as others
//
// Thread should read configuration from its arguments, then send a packet using srv6 if specified.
// Thread should then wait for a response. Upon receiving it, it puts some info into another TSRB and we can get that later

#include "net/gnrc/ipv6/hdr.h"
#include "net/gnrc/nettype.h"
#include "net/gnrc/pkt.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/udp.h"

#include "src/board_config.h"
#include "src/configs/config_common.h"
#include "ztimer.h"

#include "records.h"
#include "sendrecv.h"
#include "srv6.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#define SENDRECV_THREAD_PRIORITY (THREAD_PRIORITY_MAIN - 3)
// Stack for the stats thread
static char _stack[THREAD_STACKSIZE_MEDIUM];

// Message queue for receiving packets
#define QUEUE_SIZE 8
static msg_t _msg_q[QUEUE_SIZE];

// How many milliseconds to wait before sender assumes the receiver didn't get their message
#define SENDER_TIMEOUT_MS 5000

static gnrc_pktsnip_t *build_pkt_ipv6(unsigned int dest_id, struct node_configuration *config)
{
    const char msg[] = "hello world";

    ipv6_addr_t source_addr = get_node_addr(config->this_id, config);
    unsigned int source_port = get_node_port(config->this_id, config);

    ipv6_addr_t dest_addr = get_node_addr(dest_id, config);
    unsigned int dest_port = get_node_port(dest_id, config);

    gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, msg, sizeof(msg), GNRC_NETTYPE_UNDEF);
    if (pkt == NULL) {
        DEBUG("Error: unable to copy data to packet buffer\n");
        return NULL;
    }
    gnrc_pktsnip_t *udp_hdr = gnrc_udp_hdr_build(pkt, source_port, dest_port);
    if (pkt == NULL) {
        DEBUG("Error: unable to allocate UDP header\n");
        gnrc_pktbuf_release(pkt);
        return NULL;
    }
    gnrc_pktsnip_t *ip_hdr = gnrc_ipv6_hdr_build(udp_hdr, &source_addr, &dest_addr);
    if (ip_hdr == NULL) {
        DEBUG("Error: unable to allocate IPv6 header\n");
        gnrc_pktbuf_release(udp_hdr);
        return NULL;
    }

    return ip_hdr;
}

static unsigned int count_segments(char *segments)
{
    unsigned int count = 0;
    char *ptr = segments;
    char *endptr = NULL;
    while (*ptr != '\0') {
        (void)strtol(ptr, &endptr, 10);
        // Means no numbers left to parse
        if (endptr == ptr) {
            break;
        }
        count++;
        // strtol sets endptr to the first char after a number (ignoring preceeding whitespace)
        // So setting ptr to endptr will skip over previous number, or until end of string
        ptr = endptr;
    }
    return count;
}

static gnrc_pktsnip_t *build_pkt_srv6(struct srv6_route *route, struct node_configuration *config)
{
    const char msg[] = "hello world";

    unsigned int num_segments = 1 + count_segments(route->segments);

    // Dest address makes at least one segment

    DEBUG("Sending with %u segments\n", num_segments);

    unsigned int dest_port = get_node_port(route->dest_id, config);
    unsigned int source_port = get_node_port(config->this_id, config);
    gnrc_pktsnip_t *pkt = srv6_pkt_init(source_port, dest_port, num_segments, msg, sizeof(msg));

    unsigned int segment_num = 0;
    char *ptr = route->segments;
    char *endptr = NULL;
    while (*ptr != '\0') {
        unsigned long node_id = strtoul(ptr, &endptr, 10);

        // Means no numbers left to parse
        if (endptr == ptr) {
            break;
        }

        DEBUG("Adding segment for node %ld\n", node_id);
        ipv6_addr_t segment = get_node_addr(node_id, config);
        srv6_pkt_set_segment(pkt, &segment, segment_num++);

        // strtol sets endptr to the first char after a number (ignoring preceeding whitespace)
        // So setting ptr to endptr will skip over previous number, or until end of string
        ptr = endptr;
    }

    ipv6_addr_t dest_addr = get_node_addr(route->dest_id, config);
    srv6_pkt_set_segment(pkt, &dest_addr, segment_num);

    ipv6_addr_t source_addr = get_node_addr(config->this_id, config);
    // Will now include the ipv6 header on the outside
    pkt = srv6_pkt_complete(pkt, &source_addr);

    debug_print_snip_chain("", pkt);

    return pkt;
}

static int send(unsigned int dest_id, struct node_configuration *config)
{
    gnrc_pktsnip_t *pkt = NULL;

    // Check if there's an appropriate route to use
    struct srv6_route *route = get_srv6_route(config->this_id, dest_id, config);
    if (config->use_srv6 && route) {
        pkt = build_pkt_srv6(route, config);
    }
    else {
        pkt = build_pkt_ipv6(dest_id, config);
    }

    // We were unable to build a packet
    if (!pkt) {
        return -1;
    }

    // Send, assuming the correct interface will be used
    if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_IPV6, GNRC_NETREG_DEMUX_CTX_ALL, pkt)) {
        DEBUG("Error: unable to locate UDP thread\n");
        gnrc_pktbuf_release(pkt);
        return -1;
    }
    DEBUG("Sent a packet\n");

    return 0;
}

void debug_print_pkt(gnrc_pktsnip_t *pkt)
{
    (void)pkt;
#if ENABLE_DEBUG == 1
    gnrc_pktsnip_t *udp_hdr = pkt->next;
    udp_hdr_t *udp_header = udp_hdr->data;
    DEBUG("Packet UDP header src %u dst %u\n", byteorder_ntohs(udp_header->src_port), byteorder_ntohs(udp_header->dst_port));

    gnrc_pktsnip_t *ipv6_hdr = udp_hdr->next;
    if (ipv6_hdr->type == GNRC_NETTYPE_IPV6_EXT) {
        // TODO: Can add printing for segment routing header here, for now just skip
        ipv6_hdr = ipv6_hdr->next;
    }

    ipv6_hdr_t *ipv6_header = ipv6_hdr->data;

    DEBUG("Packet IPv6 header src ");
    ipv6_addr_print(&ipv6_header->src);
    DEBUG(" dst ");
    ipv6_addr_print(&ipv6_header->dst);
    DEBUG("\n");
#endif
}

int is_pkt_valid(gnrc_pktsnip_t *pkt)
{
    gnrc_pktsnip_t *next_hdr = pkt->next;
    if (next_hdr && next_hdr->type == GNRC_NETTYPE_UDP) {
        next_hdr = next_hdr->next;
        if (next_hdr) {
            if (next_hdr->type == GNRC_NETTYPE_IPV6_EXT) {
                next_hdr = next_hdr->next;
            }
            if (next_hdr->type == GNRC_NETTYPE_IPV6) {
                return 1;
            }
            DEBUG("Didn't get an IPv6 packet as expected\n");
        }
    }
    return 0;
}

gnrc_pktsnip_t *recv(unsigned int timeout)
{
    msg_t msg;
    msg_t reply;
    reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
    reply.content.value = -ENOTSUP;

    while (1) {
        if (timeout > 0) {
            if (ztimer_msg_receive_timeout(ZTIMER_MSEC, &msg, timeout) == -ETIME) {
                return NULL;
            }
        }
        else {
            msg_receive(&msg);
        }
        switch (msg.type) {
        case GNRC_NETAPI_MSG_TYPE_RCV: {
            gnrc_pktsnip_t *pkt = msg.content.ptr;
            if (is_pkt_valid(pkt)) {
                return pkt;
            }
            break;
        }
        case GNRC_NETAPI_MSG_TYPE_SND:
            // This should never happen, because we're the only sender. For now just ignore it
            gnrc_pktbuf_release(msg.content.ptr);
            break;
        // These case statements aren't needed for just watching for send/recv
        case GNRC_NETAPI_MSG_TYPE_GET:
        case GNRC_NETAPI_MSG_TYPE_SET:
            msg_reply(&msg, &reply);
            break;
        default:
            break;
        }
        DEBUG("Continuing to receive\n");
    }
}

static void *_sender_loop(void *ctx)
{
    struct sendrecv_thread_args *args = ctx;

    while (1) {
        ztimer_now_t start = ztimer_now(ZTIMER_MSEC);
        send(get_node_traffic_config(args->config)->dest_id, args->config);

        DEBUG("Starting receive\n");
        gnrc_pktsnip_t *pkt = recv(SENDER_TIMEOUT_MS);
        if (pkt == NULL) {
            DEBUG("No response, timed out\n");
            ztimer_now_t end = ztimer_now(ZTIMER_MSEC);
            struct latency_record latency = { .time = end, .type = LATENCY_RECORD_TYPE_NOT_RECEIVED, .round_trip_time = end - start };
            add_record(args->latency_tsrb, (unsigned char *)&latency, sizeof(latency));
            continue;
        }

        DEBUG("Got a response\n");
        debug_print_pkt(pkt);
        // For now, just assume this is a correct reponse. Could check a magic number or data later
        gnrc_pktbuf_release(pkt);
        // Getting our time from the ztimer instead of the packet means our time will be consistent with
        // the rest of the times we are measuring
        ztimer_now_t end = ztimer_now(ZTIMER_MSEC);
        ztimer_now_t round_trip = end - start;
        struct latency_record latency = { .time = end, .type = LATENCY_RECORD_TYPE_NOMINAL, .round_trip_time = round_trip };
        add_record(args->latency_tsrb, (unsigned char *)&latency, sizeof(latency));
        DEBUG("Added a latency record with round trip time %" PRIu32 " ms\n", round_trip);
        ztimer_sleep(ZTIMER_MSEC, 1000);
    }
    return NULL;
}

static void *_receiver_loop(void *ctx)
{
    struct sendrecv_thread_args *args = ctx;

    while (1) {
        DEBUG("Waiting for an incoming packet\n");
        gnrc_pktsnip_t *pkt = recv(0);
        DEBUG("Got a packet\n");
        debug_print_pkt(pkt);

        // Assume packet has been verified to be correct
        gnrc_pktsnip_t *udp_hdr = pkt->next;
        udp_hdr_t *udp_header = udp_hdr->data;
        gnrc_pktsnip_t *ipv6_hdr = udp_hdr->next;
        if (ipv6_hdr->type == GNRC_NETTYPE_IPV6_EXT) {
            // Skip the SRH for now
            ipv6_hdr = ipv6_hdr->next;
        }
        ipv6_hdr_t *ipv6_header = ipv6_hdr->data;
        (void)udp_header;

        // This could probably just be replaced with dest_id from traffic configuration,
        // but because it's a more general solution to can leave it
        int src_id = get_node_id(&ipv6_header->src, args->config);
        if (src_id < 0) {
            // This might happen if there are misconfigured nodes, or other traffic
            DEBUG("Got a message from an unknown node\n");
        }
        else {
            send(src_id, args->config);
            DEBUG("Sent a response back\n");
        }
        // Make sure we release the packet's data
        gnrc_pktbuf_release(pkt);
    }
    return NULL;
}

// A seperate thread is required here to use the assigned thread PID in some setup
static void *_sendrecv_thread_init(void *ctx)
{
    struct sendrecv_thread_args *args = ctx;

    // Set up UDP receive for both roles
    msg_init_queue(_msg_q, QUEUE_SIZE);
    gnrc_netreg_entry_t me_reg = GNRC_NETREG_ENTRY_INIT_PID(get_node_addr_config(args->config)->port,
                                                            thread_getpid());
    gnrc_netreg_register(GNRC_NETTYPE_UDP, &me_reg);

    switch (get_node_traffic_config(args->config)->role) {
    case NODE_ROLE_SENDER:
        return _sender_loop(args);
        break;
    case NODE_ROLE_RECIEVER:
        return _receiver_loop(args);
        break;
    default:
        // If this node is NODE_ROLE_FORWARD_ONLY, don't start any threads
        break;
    }
    return NULL;
}

void init_sendrecv_thread(struct sendrecv_thread_args *args)
{
    thread_create(_stack, sizeof(_stack), SENDRECV_THREAD_PRIORITY, 0, _sendrecv_thread_init, args, "sendrecv");
}
