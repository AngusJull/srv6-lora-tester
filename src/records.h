#ifndef _RECORDS_H_
#define _RECORDS_H_

#include "mutex.h"
#include "stdint.h"
#include "inttypes.h"

typedef uint32_t stat_time_t;

#ifndef STAT_TIME_FMT
#  define STAT_TIME_FMT PRIu32
#endif

// Record list, implementing the very basics of a vector, pretty much
struct record_list {
    mutex_t mutex;
    uint8_t *elements;
    unsigned int len;
    unsigned int capacity;
    size_t record_size;
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

    // netstats come with other fields that don't seem to be used properly by the board
    uint32_t tx_unicast_count;
    uint32_t tx_bytes;
    uint32_t rx_count;
    uint32_t rx_bytes;
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
    uint8_t segments_left;
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

// Record list, implementing the very basics of a vector, pretty much
struct record_list {
    mutex_t mutex;
    uint8_t *backing;
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
void print_power_record(struct power_record *record);
void print_netstat_record(struct netstat_record *record);
void print_capture_record(struct capture_record *record);
void print_latency_record(struct latency_record *record);

#endif // _RECORDS_H_
