#ifndef _SRV6_H_
#define _SRV6_H_

#include "net/gnrc/pkt.h"
#include "net/gnrc/srv6/srh.h"
#include "net/ipv6/addr.h"
#include "stddef.h"
#include "stdio.h"
#include "string.h"

gnrc_pktsnip_t *srv6_pkt_init(uint16_t src_port, uint16_t dst_port, unsigned int num_segments, const void *payload, size_t payload_len);
void srv6_pkt_set_segment(gnrc_pktsnip_t *srv6_hdr, ipv6_addr_t *segment, unsigned int segment_num);
gnrc_pktsnip_t *srv6_pkt_complete(gnrc_pktsnip_t *srv6_hdr, ipv6_addr_t *src);
int srv6_pkt_send(gnrc_pktsnip_t *ip_hdr);

#endif /* _SRV6_H_ */
