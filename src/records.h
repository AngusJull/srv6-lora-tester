#ifndef _RECORDS_H_
#define _RECORDS_H_

#include "mutex.h"
#include "stdint.h"
#include "inttypes.h"

typedef uint32_t stat_time_t;

#ifndef STAT_TIME_FMT
#  define STAT_TIME_FMT PRIu32
#endif

struct netstats {
    uint32_t tx_bytes;
    uint32_t rx_bytes;
    uint16_t tx_unicast_count;
    uint16_t rx_count;
};

struct stats_record {
    stat_time_t time;
    struct netstats l2;
    struct netstats l3;
    // netstats come with other fields that don't seem to be used properly by the board
    uint16_t millivolts;
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
    uint16_t headers_len;
    uint16_t payload_len;
    uint8_t event_type;
    uint8_t packet_type;
    uint8_t segments_left; // The number of segments left, if this packet is an SRv6 packet
    int8_t dest_id;        // The current destination ID for this packet, if it is an IPv6 or SRv6 packet
};

enum latency_record_type {
    LATENCY_RECORD_TYPE_NOT_RECEIVED,
    LATENCY_RECORD_TYPE_NOMINAL,
};

struct latency_record {
    stat_time_t time;
    stat_time_t round_trip_time;
    uint8_t type;
};

// Stores multiple records
struct record_list {
    mutex_t mutex;
    uint8_t *backing;
    unsigned int write_idx;
    unsigned int len;
    unsigned int capacity;
    size_t record_size;
};

void record_list_init(struct record_list *list, uint8_t *backing, unsigned int backing_size, unsigned int record_size);
int record_list_insert(struct record_list *list, uint8_t *record, size_t record_size);
void record_list_iter(struct record_list *list, int (*func)(uint8_t *record, size_t record_size, void *ctx), void *ctx);
int record_list_first(struct record_list *list, uint8_t *record, size_t record_size);
unsigned int record_list_len(struct record_list *list);
int record_list_clear(struct record_list *list);

void print_record_list_json_array(struct record_list *list, void (*print_func)(void *, size_t));
void print_stats_record(struct stats_record *record);
void print_capture_record(struct capture_record *record);
void print_latency_record(struct latency_record *record);

#endif // _RECORDS_H_
