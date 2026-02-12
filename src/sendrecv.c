// Create a  thread with the same priority as others
//
// Thread should read configuration from its arguments, then send a packet using srv6 if specified.
// Thread should then wait for a response. Upon receiving it, it puts some info into another TSRB and we can get that later

#include "net/gnrc/ipv6/hdr.h"
#include "net/gnrc/pkt.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/udp.h"

#include "src/netstats.h"
#include "stats.h"
#include "ztimer.h"

#include "sendrecv.h"

#define ENABLE_DEBUG 1
#include "debug.h"

#define SENDRECV_THREAD_PRIORITY (THREAD_PRIORITY_MAIN - 3)
// Stack for the stats thread
static char _stack[THREAD_STACKSIZE_MEDIUM];

// Message queue for receiving packets
#define QUEUE_SIZE 8
static msg_t _msg_q[QUEUE_SIZE];

// How many milliseconds to wait before sender assumes the receiver didn't get their message
#define SENDER_TIMEOUT_MS 5000

int send(gnrc_netif_t *netif, ipv6_addr_t *source_addr, ipv6_addr_t *dest_addr, unsigned int dest_port, unsigned int source_port)
{
    uint32_t data = 32;

    gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, &data, sizeof(data), GNRC_NETTYPE_UNDEF);
    if (pkt == NULL) {
        DEBUG("Error: unable to copy data to packet buffer\n");
        return -1;
    }
    gnrc_pktsnip_t *udp_hdr = gnrc_udp_hdr_build(pkt, source_port, dest_port);
    if (pkt == NULL) {
        DEBUG("Error: unable to allocate UDP header\n");
        gnrc_pktbuf_release(pkt);
        return -1;
    }
    gnrc_pktsnip_t *ip_hdr = gnrc_ipv6_hdr_build(udp_hdr, source_addr, dest_addr);
    if (ip_hdr == NULL) {
        DEBUG("Error: unable to allocate IPv6 header\n");
        gnrc_pktbuf_release(udp_hdr);
        return -1;
    }

    gnrc_pktsnip_t *netif_hdr = gnrc_netif_hdr_build(NULL, 0, NULL, 0);
    if (netif_hdr == NULL) {
        DEBUG("Unable to allocate netif header\n");
        gnrc_pktbuf_release(ip_hdr);
        return -1;
    }

    gnrc_netif_hdr_set_netif(netif_hdr->data, netif);
    ip_hdr = gnrc_pkt_prepend(ip_hdr, netif_hdr);

    if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_UDP, GNRC_NETREG_DEMUX_CTX_ALL, ip_hdr)) {
        DEBUG("Error: unable to locate UDP thread\n");
        gnrc_pktbuf_release(ip_hdr);
        return -1;
    }
    DEBUG("Built and sent a packet\n");

    return 0;
}

void debug_print_pkt(gnrc_pktsnip_t *pkt)
{
#if ENABLE_DEBUG == 1
    gnrc_pktsnip_t *udp_hdr = pkt->next;
    udp_hdr_t *udp_header = udp_hdr->data;
    gnrc_pktsnip_t *ipv6_hdr = udp_hdr->next;
    ipv6_hdr_t *ipv6_header = ipv6_hdr->data;

    DEBUG("Packet UDP header src %u dst %u\n", byteorder_ntohs(udp_header->src_port), byteorder_ntohs(udp_header->dst_port));
    DEBUG("Packet IPv6 header src ");
    ipv6_addr_print(&ipv6_header->src);
    DEBUG(" dst");
    ipv6_addr_print(&ipv6_header->dst);
#endif
}

int is_pkt_valid(gnrc_pktsnip_t *pkt)
{
    gnrc_pktsnip_t *udp_hdr = pkt->next;
    if (!(udp_hdr && udp_hdr->type == GNRC_NETTYPE_UDP)) {
        DEBUG("Didn't get a UDP packet as expected\n");
        return 0;
    }
    gnrc_pktsnip_t *ipv6_hdr = udp_hdr->next;
    if (!(ipv6_hdr && ipv6_hdr->type == GNRC_NETTYPE_IPV6)) {
        DEBUG("Didn't get an IPv6 packet as expected\n");
        return 0;
    }
    return 1;
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
    gnrc_netif_t *radio = get_lora_netif();
    ipv6_addr_t source_addr = get_node_addr(args->config->this_id);
    ipv6_addr_t dest_addr = get_node_addr(args->config->traffic_config.dest_id);
    unsigned int dest_port = get_node_port(args->config->traffic_config.dest_id);

    while (1) {
        ztimer_now_t start = ztimer_now(ZTIMER_MSEC);
        send(radio, &source_addr, &dest_addr, dest_port, args->config->addr_config.port);

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
    gnrc_netif_t *radio = get_lora_netif();
    ipv6_addr_t source_addr = get_node_addr(args->config->this_id);

    while (1) {
        DEBUG("Waiting for an incoming packet\n");
        gnrc_pktsnip_t *pkt = recv(0);
        DEBUG("Got a packet\n");
        debug_print_pkt(pkt);

        // Assume packet has been verified to be correct
        gnrc_pktsnip_t *udp_hdr = pkt->next;
        udp_hdr_t *udp_header = udp_hdr->data;
        gnrc_pktsnip_t *ipv6_hdr = udp_hdr->next;
        ipv6_hdr_t *ipv6_header = ipv6_hdr->data;

        // We need to change the byte orders back to host order because the UDP header creation
        // expects it that way
        send(radio, &source_addr, &ipv6_header->src, byteorder_ntohs(udp_header->src_port), byteorder_ntohs(udp_header->dst_port));
        DEBUG("Sent a response back");
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
    gnrc_netreg_entry_t me_reg = GNRC_NETREG_ENTRY_INIT_PID(args->config->addr_config.port,
                                                            thread_getpid());
    gnrc_netreg_register(GNRC_NETTYPE_UDP, &me_reg);

    switch (args->config->traffic_config.role) {
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
