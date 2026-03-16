#include "net/gnrc/pktbuf.h"
#include "net/gnrc/netif.h"
#include "net/ipv6/hdr.h"
#include "net/gnrc/ipv6/hdr.h"
#include "net/gnrc/srv6/srh.h"
#include "net/protnum.h"
#include "net/gnrc/udp.h"
#include "net/udp.h"

#include "srv6.h"

gnrc_pktsnip_t *srv6_pkt_init(uint16_t src_port, uint16_t dst_port, unsigned int num_segments, const void *payload, size_t payload_len)
{
    // allocate udp request
    gnrc_pktsnip_t *payload_snip = gnrc_pktbuf_add(NULL, payload, payload_len, GNRC_NETTYPE_UNDEF);
    if (payload_snip == NULL) {
        printf("Failed to allocate UDP payload\n");
        return NULL;
    }

    // Build UDP header - can't send into UDP stack so must set other fields manually
    gnrc_pktsnip_t *udp_snip = gnrc_udp_hdr_build(payload_snip, src_port, dst_port);
    if (!udp_snip) {
        printf("Failed to allocate UDP packet\n");
        gnrc_pktbuf_release(payload_snip);
        return NULL;
    }
    udp_hdr_t *udp = udp_snip->data;
    udp->length = byteorder_htons(gnrc_pkt_len(udp_snip));

    // allocate packet buffer for SRH
    size_t srh_size = sizeof(gnrc_srv6_srh_t) + (num_segments * sizeof(ipv6_addr_t));
    gnrc_pktsnip_t *srh_snip = gnrc_pktbuf_add(udp_snip, NULL, srh_size, GNRC_NETTYPE_IPV6_EXT);
    if (srh_snip == NULL) {
        printf("Failed to allocate SRH\n");
        gnrc_pktbuf_release(udp_snip);
        return NULL;
    }

    gnrc_srv6_srh_t *srh = srh_snip->data;
    srh->nh = PROTNUM_UDP;
    srh->len = (srh_size - 8) / 8; // length in 8-octet units, excluding first 8
    srh->type = IPV6_EXT_RH_TYPE_SRV6;
    srh->seg_left = num_segments - 1; // length in 8-octet units, excluding first 8
    srh->last_entry = num_segments - 1;
    srh->flags = 0;
    srh->tag = 0;

    return srh_snip;
}

// Set segments in the SRH, where segment_num 0 is the first hop, and segment_num seg_left - 1 is the destination
void srv6_pkt_set_segment(gnrc_pktsnip_t *srv6_hdr, ipv6_addr_t *segment, unsigned int segment_num)
{
    gnrc_srv6_srh_t *srh = srv6_hdr->data;

    // Segments start immediately after the srh structure
    ipv6_addr_t *segments = (ipv6_addr_t *)(srh + 1);
    // Start from the end of the segment list, work backwards. Assume we will get the correct number of segments set
    assert(segment_num <= srh->last_entry);
    unsigned int index = srh->last_entry - segment_num;
    segments[index] = *segment;
}

gnrc_pktsnip_t *srv6_pkt_complete(gnrc_pktsnip_t *srv6_hdr, ipv6_addr_t *src)
{
    gnrc_srv6_srh_t *srh = srv6_hdr->data;
    // Segments start immediately after the srh structure
    ipv6_addr_t *segments = (ipv6_addr_t *)(srh + 1);
    // initial IPv6 destination is the first hop
    ipv6_addr_t dest = segments[srh->last_entry];

    // add IPv6 header w/ first segment as dest
    // gnrc_ipv6_hdr_build will auto-set nh field from next snip type
    gnrc_pktsnip_t *ipv6_snip = gnrc_ipv6_hdr_build(srv6_hdr, src, &dest);
    if (ipv6_snip == NULL) {
        printf("Failed to allocate IPv6 header\n");
        gnrc_pktbuf_release(srv6_hdr);
        return NULL;
    }

    // Create pseudo IPv6 header with final dest for checksum. Need to do this because
    // the IPv6 header doesn't contain the final source & dest when using SRv6
    // TODO: may want to swap this with a regular gnrc_ipv6_hdr_build
    gnrc_pktsnip_t *pseudo_ipv6_snip = gnrc_pktbuf_add(NULL, ipv6_snip->data,
                                                       ipv6_snip->size,
                                                       GNRC_NETTYPE_IPV6);
    if (pseudo_ipv6_snip == NULL) {
        printf("Failed to allocate pseudo IPv6 header\n");
        gnrc_pktbuf_release(ipv6_snip);
        return NULL;
    }
    // Check later if we need to set other fields on this pseudo-header
    ipv6_hdr_t *pseudo_hdr = pseudo_ipv6_snip->data;
    memcpy(&pseudo_hdr->dst, &dest, sizeof(ipv6_addr_t));

    gnrc_pktsnip_t *udp_snip = srv6_hdr->next;
    assert(udp_snip->type == GNRC_NETTYPE_UDP);
    if (gnrc_udp_calc_csum(udp_snip, pseudo_ipv6_snip) < 0) {
        printf("Failed to calculate UDP checksum\n");
        gnrc_pktbuf_release(ipv6_snip);
        return NULL;
    }
    gnrc_pktbuf_release(pseudo_ipv6_snip);

    return ipv6_snip;
}

int srv6_pkt_send(gnrc_pktsnip_t *ip_hdr)
{
    // send packet to IPv6 layer for transmission
    if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_IPV6, GNRC_NETREG_DEMUX_CTX_ALL, ip_hdr)) {
        printf("Failed to send packet\n");
        gnrc_pktbuf_release(ip_hdr);
        return 1;
    }
    return 0;
}
