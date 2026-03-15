#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#include "records.h"

#define RECORD_LIST_INIT_CAPACITY 4

// Initalize a new list with no elements
void record_list_init(struct record_list *list, uint8_t *backing, unsigned int backing_size, unsigned int record_size)
{
    mutex_init(&list->mutex);
    mutex_lock(&list->mutex);

    list->record_size = record_size;
    list->len = 0;
    list->capacity = backing_size / record_size;
    list->backing = backing;

    mutex_unlock(&list->mutex);
}

// Add an element to the front of the list by allocating a new element and copying some data into it
// Return 0 if the element was added, or 1 otherwise
int record_list_insert(struct record_list *list, uint8_t *record, size_t record_size)
{
    assert(record_size == list->record_size);

    int ret = 0;
    mutex_lock(&list->mutex);

    // If we end up out of space, just ignore. Silent so we can still get information off the boards
    if (list->len >= list->capacity) {
        ret = 1;
    }
    else {
        // Actually put the element on the end, but consistently use the list as if it goes
        // on the front so just use that terminology
        memcpy(&list->backing[list->len * list->record_size], record, record_size);
        list->len++;
    }

    mutex_unlock(&list->mutex);

    return ret;
}

// The provided function is called with the given context and the record pointer for each element
// Iteration stops if the provided function does not return zero
// In the future, this could be extended to provide a way to remove the current element if needed
void record_list_iter(struct record_list *list, int (*func)(uint8_t *record, size_t record_size, void *ctx), void *ctx)
{
    mutex_lock(&list->mutex);

    // Iterate backwards so we see the newest element first and oldest element last,
    // since all other operations start at the end or modify the end
    for (unsigned int i = list->len; i-- > 0;) {
        if (func(&list->backing[i * list->record_size], list->record_size, ctx)) {
            break;
        }
    }

    mutex_unlock(&list->mutex);
}

// Copy out the first element of this list and return 0, or if it is empty return 1
int record_list_first(struct record_list *list, uint8_t *record, size_t record_size)
{
    assert(record_size == list->record_size);

    int ret = 0;
    mutex_lock(&list->mutex);

    if (list->len) {
        memcpy(record, &list->backing[(list->len - 1) * list->record_size], record_size);
    }
    else {
        ret = 1;
    }

    mutex_unlock(&list->mutex);

    return ret;
}

// Get the length of the list
unsigned int record_list_len(struct record_list *list)
{
    unsigned int len;
    mutex_lock(&list->mutex);

    len = list->len;

    mutex_unlock(&list->mutex);

    return len;
}

// Remove all elements from the list
int record_list_clear(struct record_list *list)
{
    mutex_lock(&list->mutex);

    list->len = 0;

    mutex_unlock(&list->mutex);

    return 0;
}

struct print_record_list_json_array_inner_ctx {
    unsigned int index;
    void (*print_func)(void *, size_t);
};

static int _print_record_list_json_array_inner(uint8_t *data, size_t data_len, void *ctx)
{
    struct print_record_list_json_array_inner_ctx *args = ctx;

    // Prevent trailing comma that is invalid JSON
    if (args->index > 0) {
        printf(",");
    }
    args->index++;
    args->print_func(data, data_len);

    return 0;
}

void print_record_list_json_array(struct record_list *list, void (*print_func)(void *, size_t))
{
    printf("[");
    struct print_record_list_json_array_inner_ctx args = { .index = 0, .print_func = print_func };
    record_list_iter(list, _print_record_list_json_array_inner, &args);
    printf("]");
}

static void print_netstats(char *prefix, struct netstats *stats)
{
    printf("\"%s_tx_cnt\":%" PRIu16
           ",\"%s_tx_byt\":%" PRIu32
           ",\"%s_rx_cnt\":%" PRIu16
           ",\"%s_rx_byt\":%" PRIu32,
           prefix,
           stats->tx_unicast_count,
           prefix,
           stats->tx_bytes,
           prefix,
           stats->rx_count,
           prefix,
           stats->rx_bytes);
}

void print_stats_record(struct stats_record *record)
{
    printf("{\"time\":%" STAT_TIME_FMT ",\"mv\":%" PRIu16 ",", record->time, record->millivolts);
    print_netstats("l2", &record->l2);
    printf(",");
    print_netstats("l3", &record->l3);
    printf("}");
}

void print_capture_record(struct capture_record *record)
{
    printf("{\"time\":%" STAT_TIME_FMT
           ",\"ev_type\":%d"
           ",\"pkt_type\":%d"
           ",\"hdr_len\":%" PRIu16
           ",\"pld_len\":%" PRIu16
           ",\"seg_lft\":%u}",
           record->time,
           record->event_type,
           record->packet_type,
           record->headers_len,
           record->payload_len,
           record->segments_left);
}

void print_latency_record(struct latency_record *record)
{
    printf("{\"time\":%" STAT_TIME_FMT
           ",\"type\":%d"
           ",\"rt_time\":%" STAT_TIME_FMT "}",
           record->time,
           record->type,
           record->round_trip_time);
}
