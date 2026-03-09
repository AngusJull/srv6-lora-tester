#include "srv6.h"
static char gnrc_udp_server_stack[THREAD_STACKSIZE_DEFAULT];

void debug_print_snip_chain(const char *msg, gnrc_pktsnip_t *pkt) {
    printf("[SRv6 example] %s: snip chain: ", msg);
    while (pkt) {
        printf("[%d:%u]->", pkt->type, (unsigned)pkt->size);
        pkt = pkt->next;
    }
    puts("NULL");
}

static void *gnrc_udp_server_thread(void *arg)
{
    (void)arg;
    msg_t msg;
    gnrc_netreg_entry_t entry = GNRC_NETREG_ENTRY_INIT_PID(SERVER_PORT, thread_getpid());

    static msg_t server_msg_queue[MAIN_QUEUE_SIZE];
    msg_init_queue(server_msg_queue, MAIN_QUEUE_SIZE);

    // register to recieve UDP packets from server port
    gnrc_netreg_register(GNRC_NETTYPE_UDP, &entry);

    puts("GNRC UDP server started.");

    while (1) {
        msg_receive(&msg);
        if (msg.type == GNRC_NETAPI_MSG_TYPE_RCV) {
            gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *)msg.content.ptr;
            // extract udp payload
            gnrc_pktsnip_t *payload = gnrc_pktsnip_search_type(pkt, GNRC_NETTYPE_UNDEF);
            if (payload) {
                size_t len = payload->size;
                char buf[BUF_SIZE];
                if (len > BUF_SIZE) len = BUF_SIZE;
                memcpy(buf, payload->data, len);
                printf("Received payload of %u bytes: <\t%.*s\t>\n", (unsigned)len, (int)len, buf);
            } else {
                printf("No payload found in recieved packet.\n");
            }
            gnrc_pktbuf_release(pkt);
        }
    }
    return NULL;
}

void srv6_udp_server_start(void)
{
    thread_create(gnrc_udp_server_stack, sizeof(gnrc_udp_server_stack),
                  THREAD_PRIORITY_MAIN - 1, 0, gnrc_udp_server_thread, NULL, "gnrc_udp_srv");
}

gnrc_pktsnip_t *srv6_build_pkt(ipv6_addr_t *dest,
                                char **seg_args, int num_segments,
                                uint16_t src_port, uint16_t dst_port,
                                const void *payload, size_t payload_len)
{
    // allocate udp request
    gnrc_pktsnip_t *payload_snip = gnrc_pktbuf_add(NULL, payload, payload_len, GNRC_NETTYPE_UNDEF);
    if (payload_snip == NULL) {
        printf("Failed to allocate UDP payload\n");
        return NULL;
    }

    // build UDP header
    gnrc_pktsnip_t *udp_snip = gnrc_udp_hdr_build(payload_snip, src_port, dst_port);
    if (!udp_snip) {
        printf("Failed to allocate UDP packet\n");
        gnrc_pktbuf_release(payload_snip);
        return NULL;
    }
    udp_hdr_t *udp = udp_snip->data;
    udp->length = byteorder_htons(gnrc_pkt_len(udp_snip));

    // allocate packet buffer for SRH
    size_t srh_size = sizeof(gnrc_srv6_srh_t) + num_segments * sizeof(ipv6_addr_t);
    gnrc_pktsnip_t *srh_snip = gnrc_pktbuf_add(udp_snip, NULL, srh_size, GNRC_NETTYPE_IPV6_EXT);
    if (srh_snip == NULL) {
        printf("Failed to allocate SRH\n");
        gnrc_pktbuf_release(udp_snip);
        return NULL;
    }

    gnrc_srv6_srh_t *srh = srh_snip->data;
    srh->nh         = PROTNUM_UDP;
    srh->len        = (srh_size - 8) / 8; // length in 8-octet units, excluding first 8
    srh->type       = IPV6_EXT_RH_TYPE_SRV6;
    srh->seg_left   = num_segments - 1;  // length in 8-octet units, excluding first 8
    srh->last_entry = num_segments - 1;
    srh->flags      = 0;
    srh->tag        = 0;

    // segment_list[0] = final destination; segment_list[last_entry] = first hop
    ipv6_addr_t *segments = (ipv6_addr_t *)(srh + 1);
    memcpy(&segments[0], dest, sizeof(ipv6_addr_t));

    for (int i = 0; i < num_segments-1; i++) {
        int seg_idx = num_segments-1 - i;      // fill from the end backwards
        if (ipv6_addr_from_str(&segments[seg_idx], seg_args[2 + i]) == NULL) {
            printf("Invalid segment address %s\n", seg_args[2 + i]);
            return NULL;
        }
    }

    // set real source address for correct checksum
    gnrc_netif_t *netif = gnrc_netif_iter(NULL);
    ipv6_addr_t addrs[CONFIG_GNRC_NETIF_IPV6_ADDRS_NUMOF];
    int num_addrs = gnrc_netif_ipv6_addrs_get(netif, addrs, sizeof(addrs));
    ipv6_addr_t ipv6_src;
    if (num_addrs > 0) {
        ipv6_src = addrs[0];
        for (int i = 0; i < (int)(num_addrs / sizeof(ipv6_addr_t)); i++) {
            if (ipv6_addr_is_global(&addrs[i])) {
                ipv6_src = addrs[i];
                break;
            }
        }
    } else { printf("Checksum generation failed. No global source address found\n"); }

    // initial IPv6 destination is the first hop
    ipv6_addr_t ipv6_dest = segments[srh->last_entry];

     // add IPv6 header w/ first segment as dest
    // gnrc_ipv6_hdr_build will auto-set nh field from next snip type
    gnrc_pktsnip_t *ipv6_snip = gnrc_ipv6_hdr_build(srh_snip, &ipv6_src, &ipv6_dest);
    if (ipv6_snip == NULL) {
        printf("Failed to allocate IPv6 header\n");
        gnrc_pktbuf_release(srh_snip);
        return NULL;
    } 

    // create pseudo IPv6 header with final dest for checksum
    gnrc_pktsnip_t *pseudo_ipv6_snip = gnrc_pktbuf_add(NULL, ipv6_snip->data,
                                                        ipv6_snip->size,
                                                        GNRC_NETTYPE_IPV6);
    if (pseudo_ipv6_snip == NULL) {
        printf("Failed to allocate pseudo IPv6 header\n");
        gnrc_pktbuf_release(ipv6_snip);
        return NULL;
    }
    ipv6_hdr_t *pseudo_hdr = pseudo_ipv6_snip->data;
    memcpy(&pseudo_hdr->dst, dest, sizeof(ipv6_addr_t));

    // calculate udp checksum
    if (gnrc_udp_calc_csum(udp_snip, pseudo_ipv6_snip) < 0) {
        printf("Failed to calculate UDP checksum\n");
    }
    gnrc_pktbuf_release(pseudo_ipv6_snip);

    return ipv6_snip;
}