#ifndef _RECORDS_H_
#define _RECORDS_H_

#include "mutex.h"
#include "stdint.h"
#include "inttypes.h"
#include "tsrb.h"

typedef uint32_t stat_time_t;

#ifndef STAT_TIME_FMT
#  define STAT_TIME_FMT PRIu32
#endif

// Doubly linked list element
// Using void pointer instead of something type safe because dealing with container_of and thread safety
// gets to be really cumbersome and hard to make a good interface for
struct dl_item {
    void *record;
    struct dl_item *next;
    struct dl_item *prev;
};

// Linked list with mutex to provide thread safety
struct dl_list {
    mutex_t mutex;
    struct dl_item *head;
};

struct power_record {
    stat_time_t time;
    uint32_t millivolts;
};

enum netstat_type {
    NETSTATS_RECORD_TYPE_ALL,
    NETSTATS_RECORD_TYPE_IPV6,
    NETSTATS_RECORD_TYPE_L2,
};

// Stats for L2 or L3
struct netstat_record {
    enum netstat_type type;
    stat_time_t time;

    // Copied from netstats_t, so that we can remove or add fields later
    uint32_t tx_unicast_count; /**< packets sent via unicast */
    uint32_t tx_mcast_count;   /**< packets sent via multicast (including broadcast) */
    uint32_t tx_success;       /**< successful sending operations
                                (either acknowledged or unconfirmed
                                sending operation, e.g. multicast) */
    uint32_t tx_failed;        /**< failed sending operations */
    uint32_t tx_bytes;         /**< sent bytes */
    uint32_t rx_count;         /**< received (data) packets */
    uint32_t rx_bytes;         /**< received bytes */
};

enum capture_event_type {
    CAPTURE_EVENT_TYPE_RECV,
    CAPTURE_EVENT_TYPE_SEND,
};

enum capture_packet_type {
    CAPTURE_PACKET_TYPE_SIXLOWPAN,
    CAPTURE_PACKET_TYPE_IPV6,
    CAPTURE_PACKET_TYPE_SRV6,
    CAPTURE_PACKET_TYPE_UNDEF,
};

// Stats for received and sent packets
struct capture_record {
    stat_time_t time;
    enum capture_event_type event_type;
    enum capture_packet_type packet_type;
    uint16_t headers_len;
    uint16_t payload_len;
    uint8_t segments_left;
};

enum latency_record_type {
    LATENCY_RECORD_TYPE_NOT_RECEIVED,
    LATENCY_RECORD_TYPE_NOMINAL,
};

struct latency_record {
    stat_time_t time;
    enum latency_record_type type;
    stat_time_t round_trip_time;
};

int add_record(tsrb_t *tsrb, uint8_t *record, size_t size);

void list_init(struct dl_list *list);
int list_add(struct dl_list *list, uint8_t *data, size_t data_len);
void list_iter(struct dl_list *list, int (*func)(void *record, void *ctx), void *ctx);
int list_clear(struct dl_list *list);

void print_record_json_array(tsrb_t *buffer, size_t record_len, void (*print_func)(void *, size_t));
void print_power_record(struct power_record *record);
void print_netstat_record(struct netstat_record *record);
void print_capture_record(struct capture_record *record);
void print_latency_record(struct latency_record *record);

#endif // _RECORDS_H_
