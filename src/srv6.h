#ifndef _SRV6_H_
#define _SRV6_H_

#include <stddef.h>
#include "net/ipv6/addr.h"
#include "net/gnrc.h"
#include "net/gnrc/pktdump.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/netif.h"
#include "net/ipv6/hdr.h"
#include "net/gnrc/ipv6/hdr.h"
#include "net/gnrc/srv6/srh.h"
#include "net/gnrc/icmpv6/echo.h"
#include "net/protnum.h"
#include "net/gnrc/udp.h"
#include "net/udp.h"
#include "net/gnrc/netreg.h"
#include "thread.h"

#include <stdio.h>
#include <string.h>

#include "shell.h"
#include "msg.h"

#define MAIN_QUEUE_SIZE (8)

#define BUF_SIZE 128
#define SERVER_PORT 54321
#define CLIENT_PORT 12345

void debug_print_snip_chain(const char *msg, gnrc_pktsnip_t *pkt);

void srv6_udp_server_start(void);

int srv6_send_udp(ipv6_addr_t *dest, ipv6_addr_t *segments, int num_segments,
                  uint16_t src_port, uint16_t dst_port,
                  const void *payload, size_t payload_len);

int srv6_ping_cmd(int argc, char **argv);


gnrc_pktsnip_t *srv6_build_pkt(ipv6_addr_t *dest,
                                char **seg_args, int num_segments,
                                uint16_t src_port, uint16_t dst_port,
                                const void *payload, size_t payload_len);
#endif /* _SRV6_H_ */