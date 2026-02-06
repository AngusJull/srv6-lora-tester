#include "net/gnrc/pkt.h"
#include "net/gnrc/pktbuf.h"
#include "sys/errno.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netreg.h"
#include "ztimer.h"

#define ENABLE_DEBUG 1
#include "debug.h"

#include "stats.h"
#include "pkt_capture.h"

#define PKT_CAPTURE_THREAD_PRIORITY (THREAD_PRIORITY_MAIN - 4)
// Stack for the stats thread
static char _stack[THREAD_STACKSIZE_MEDIUM];
// Size of the message queue for recieving packets
#define QUEUE_SIZE 8

enum capture_packet_type packet_type(gnrc_pktsnip_t *pkt)
{
    switch (pkt->type) {
    case GNRC_NETTYPE_NETIF:
        return CAPTURE_PACKET_TYPE_NETIF;
    case GNRC_NETTYPE_SIXLOWPAN:
        return CAPTURE_PACKET_TYPE_SIXLOWPAN;
    default:
        // If we expect to be receiving other packets to process, must add support
        printf("Got a packet type we shouldn't have been processing: %u", pkt->type);
    }
    return CAPTURE_PACKET_TYPE_UNDEF;
}

struct capture_lengths {
    uint16_t headers_len;
    uint16_t payload_len;
};

struct capture_lengths capture_headers_len(gnrc_pktsnip_t *pkt)
{
    struct capture_lengths lengths;
    while (pkt) {
        switch (pkt->type) {
        case GNRC_NETTYPE_UDP:
            // Fall-through
        case GNRC_NETTYPE_ICMPV6:
            lengths.payload_len += pkt->size;
            break;
        case GNRC_NETTYPE_IPV6:
            // Fall-through
        case GNRC_NETTYPE_SIXLOWPAN:
            lengths.headers_len += pkt->size;
            break;
        case GNRC_NETTYPE_NETIF:
        default:
            // Headers matching this don't mean anything for us.
            // TODO: Add SRv6 header
            continue;
        }
        // Look at the next header
        pkt = pkt->next;
    }
    return lengths;
}

void add_capture_record(tsrb_t *capture_tsrb, gnrc_pktsnip_t *pkt, enum capture_event_type type)
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
    add_record(capture_tsrb, (uint8_t *)&record, sizeof(record));
}

static void *_pkt_capture_loop(void *ctx)
{
    DEBUG("pkt capturing!\n");

    struct pkt_capture_thread_args *args = ctx;

    static msg_t _msg_q[QUEUE_SIZE];
    msg_t msg;

    msg_t reply;
    reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
    reply.content.value = -ENOTSUP;

    msg_init_queue(_msg_q, QUEUE_SIZE);
    gnrc_netreg_entry_t me_reg = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                            thread_getpid());

    // On line 292 of gnrc_ipv6.c we see `if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_SIXLOWPAN, GNRC_NETREG_DEMUX_CTX_ALL, pkt)) {`
    // when sending IPv6 packets
    // If we want to see packets as they are when being sent before compression, we should register for SIXLOWPAN or IPV6 types. If we want to see after fragmentation
    // and compression, should look for netif packets (which should be layer 2 and ready to be picked up by the IEEE 802.15.4 MAC)
    //
    // We can ignore received packets, since this is not useful for checking overhead or fragmentation (all relevant information can be gained from the send route)
    gnrc_netreg_register(GNRC_NETTYPE_SIXLOWPAN, &me_reg);
    gnrc_netreg_register(GNRC_NETTYPE_NETIF, &me_reg);

    while (1) {
        msg_receive(&msg);

        switch (msg.type) {
        case GNRC_NETAPI_MSG_TYPE_RCV:
            // Recieving Data. We can ignore this because we can get all the info we need out of sending only (simplifies things)
            gnrc_pktbuf_release(msg.content.ptr);
            break;
        case GNRC_NETAPI_MSG_TYPE_SND:
            // Sending data
            add_capture_record(args->capture_tsrb, msg.content.ptr, CAPTURE_EVENT_TYPE_SEND);
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
    }
    return NULL;
}

void init_pkt_capture_thread(struct pkt_capture_thread_args *args)
{
    thread_create(_stack, sizeof(_stack), PKT_CAPTURE_THREAD_PRIORITY, 0, _pkt_capture_loop, args, "packet capture");
}
