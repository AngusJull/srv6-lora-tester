#include "net/gnrc/netif/hdr.h"
#include "net/gnrc/nettype.h"
#include "net/gnrc/pkt.h"
#include "net/gnrc/pktbuf.h"
#include "net/netif.h"
#include "src/netstats.h"
#include "sys/errno.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netreg.h"
#include "ztimer.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#include "stats.h"
#include "pkt_capture.h"

#define PKT_CAPTURE_THREAD_PRIORITY (THREAD_PRIORITY_MAIN - 4)
// Stack for the stats thread
static char _stack[THREAD_STACKSIZE_MEDIUM];
// Size of the message queue for recieving packets
#define QUEUE_SIZE 8

static enum capture_packet_type packet_type(gnrc_pktsnip_t *pkt)
{
    switch (pkt->type) {
    case GNRC_NETTYPE_SIXLOWPAN:
        return CAPTURE_PACKET_TYPE_SIXLOWPAN;
    case GNRC_NETTYPE_IPV6:
        return CAPTURE_PACKET_TYPE_IPV6;
    default:
        // If we expect to be receiving other packets to process, must add support
        printf("Got a packet type we shouldn't have been processing: %d\n", pkt->type);
    }
    return CAPTURE_PACKET_TYPE_UNDEF;
}

struct capture_lengths {
    uint16_t headers_len;
    uint16_t payload_len;
};

static struct capture_lengths capture_headers_len(gnrc_pktsnip_t *pkt)
{
    struct capture_lengths lengths = { 0 };
    while (pkt) {
        DEBUG("Header type %d with length %u\n", pkt->type, pkt->size);
        switch (pkt->type) {
        case GNRC_NETTYPE_ICMPV6:
            // Fall-through
        case GNRC_NETTYPE_UNDEF:
            lengths.payload_len += pkt->size;
            break;
        case GNRC_NETTYPE_UDP:
            // Fall-through
        case GNRC_NETTYPE_IPV6:
            // Fall-through
        case GNRC_NETTYPE_SIXLOWPAN:
            lengths.headers_len += pkt->size;
            break;
        default:
            // TODO: Add SRv6 header
            break;
        }
        // Look at the next header
        pkt = pkt->next;
    }
    return lengths;
}

static void add_capture_record(tsrb_t *capture_tsrb, gnrc_pktsnip_t *pkt, enum capture_event_type type)
{
    // Can find some examples of how to work on packets in gnrc_ipv6.c:760
    // Packet snips are ordered from lowest layer to highest layer when sending, and from highest layer to lowest layer when recieving
    enum capture_packet_type pkt_type = packet_type(pkt);
    struct capture_lengths lengths = capture_headers_len(pkt);
    struct capture_record record = { .event_type = type,
                                     .packet_type = pkt_type,
                                     .headers_len = lengths.headers_len,
                                     .payload_len = lengths.payload_len,
                                     .time = ztimer_now(ZTIMER_MSEC) };
    DEBUG("Got a packet with type %d, header length %d, payload length %d\n", pkt->type, lengths.headers_len, lengths.payload_len);
    add_record(capture_tsrb, (uint8_t *)&record, sizeof(record));
}

// Make sure the packet is destined for an acceptable network interface and has a netif snip on it
static int check_pkt_netif(gnrc_pktsnip_t *pkt, int netif_pid)
{
    // It seems like all IPv6 and sixlowpan packets start with a netif header to describe their destination
    if (pkt->type == GNRC_NETTYPE_NETIF) {
        gnrc_netif_hdr_t *netif_hdr = pkt->data;
        if (netif_hdr->if_pid == netif_pid) {
            return 1;
        }
        DEBUG("Packet was destined for wrong interface and filtered: %d\n", netif_hdr->if_pid);
    }
    return 0;
}

static void *_pkt_capture_loop(void *ctx)
{
    struct pkt_capture_thread_args *args = ctx;

    int radio_pid = get_lora_netif()->pid;

    static msg_t _msg_q[QUEUE_SIZE];
    msg_t msg;

    msg_t reply;
    reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
    reply.content.value = -ENOTSUP;

    msg_init_queue(_msg_q, QUEUE_SIZE);
    gnrc_netreg_entry_t prenetif_reg = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                                  thread_getpid());

    gnrc_netreg_entry_t sixlowpan_reg = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                                   thread_getpid());

    // On line 292 of gnrc_ipv6.c we see `if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_SIXLOWPAN, GNRC_NETREG_DEMUX_CTX_ALL, pkt)) {`
    // when sending IPv6 packets
    // If we want to see packets as they are when being sent before compression, we should register for SIXLOWPAN or IPV6 types. If we want to see after fragmentation
    // and compression, should look for netif packets (which should be layer 2 and ready to be picked up by the IEEE 802.15.4 MAC)
    if (gnrc_netreg_register(GNRC_NETTYPE_SIXLOWPAN, &sixlowpan_reg) != 0) {
        DEBUG("Failed to register for sixlowpan packets\n");
    }

    // Because packets go directly from sixlowpan to their netif, this instead uses a small modification to the sixlowpan code to broadcast it
    if (gnrc_netreg_register(GNRC_NETTYPE_SIXLOWPAN_PRENETIF, &prenetif_reg) != 0) {
        DEBUG("Failed to register for sixlowpan_prenetif packets\n");
    }

    while (1) {
        msg_receive(&msg);

        switch (msg.type) {
        case GNRC_NETAPI_MSG_TYPE_RCV:
            // Recieving Data. We can ignore this because we can get all the info we need out of sending only (simplifies things)
            gnrc_pktbuf_release(msg.content.ptr);
            break;
        case GNRC_NETAPI_MSG_TYPE_SND: {
            // Sending data
            gnrc_pktsnip_t *pkt = msg.content.ptr;
            if (check_pkt_netif(pkt, radio_pid)) {
                // Remove netif header since both IPv6 and sixlowpan packets have it on
                if (pkt->type == GNRC_NETTYPE_NETIF) {
                    pkt = pkt->next;
                }
                add_capture_record(args->capture_tsrb, pkt, CAPTURE_EVENT_TYPE_SEND);
            }
            gnrc_pktbuf_release(msg.content.ptr);
            break;
        }
        // These case statements aren't needed for just watching for send/recv
        case GNRC_NETAPI_MSG_TYPE_GET:
        case GNRC_NETAPI_MSG_TYPE_SET:
            msg_reply(&msg, &reply);
            break;
        default:
            break;
        }
    }
    return NULL;
}

void init_pkt_capture_thread(struct pkt_capture_thread_args *args)
{
    thread_create(_stack, sizeof(_stack), PKT_CAPTURE_THREAD_PRIORITY, 0, _pkt_capture_loop, args, "packet capture");
}
