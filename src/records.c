#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "utlist.h"

#include "records.h"

// Initalize a new list with no elements
void list_init(struct dl_list *list)
{
    list->head = NULL;
    mutex_init(&list->mutex);
}

// Add an element to the list by allocating a new element and copying some data into it
int list_add(struct dl_list *list, uint8_t *data, size_t data_len)
{
    // Just put the data immediately after the struct
    struct dl_item *new = malloc(sizeof(*new) + data_len);
    if (new == NULL) {
        return -1;
    }

    // Copy data into record, which has memory allocated right after new
    new->record = new + 1;
    memcpy(new->record, data, data_len);

    // Only lock when we're totally ready to put in the list
    mutex_lock(&list->mutex);
    DL_PREPEND(list->head, new);
    mutex_unlock(&list->mutex);

    return 0;
}

// The provided function is called with the given context and the record pointer for each element
// Iteration stops if the provided function returns 0
// In the future, this could be extended to provide a way to remove the current element if needed
void list_iter(struct dl_list *list, int (*func)(void *record, void *ctx), void *ctx)
{
    mutex_lock(&list->mutex);

    struct dl_item *item;
    DL_FOREACH(list->head, item)
    {
        if (func(item->record, ctx) == 0) {
            return;
        }
    }

    mutex_unlock(&list->mutex);
}

int list_clear(struct dl_list *list)
{
    mutex_lock(&list->mutex);

    struct dl_item *item;
    struct dl_item *tmp;
    DL_FOREACH_SAFE(list->head, item, tmp)
    {
        // Probably don't need to bother doing this
        DL_DELETE(list->head, item);
        // Because we used just one malloc for the list item and data, can do a single free here too
        free(item);
    }
    mutex_unlock(&list->mutex);

    return 0;
}

int add_record(tsrb_t *tsrb, uint8_t *record, size_t size)
{
    // Overwrite oldest value
    if (tsrb_free(tsrb) < size) {
        // Must drop whole records so we don't corrupt data
        tsrb_drop(tsrb, size);
        // Assume this completed successfully, but if it doesn't should just be unable to add
    }
    return tsrb_add(tsrb, record, size);
}

void print_record_json_array(tsrb_t *buffer, size_t record_len, void (*print_func)(void *, size_t))
{
    puts("[");
    uint8_t record[record_len];
    while (tsrb_get(buffer, (uint8_t *)&record, record_len)) {
        print_func(&record, record_len);
        // Trailing comma at end of array is not valid JSON, so make sure there's another sample first
        if (tsrb_avail(buffer) >= sizeof(record)) {
            puts(",");
        }
    }
    puts("]");
}

void print_power_record(struct power_record *record)
{
    printf("{\"time\":%" STAT_TIME_FMT ",\"millivolts\":%" PRIu32 "}",
           record->time,
           record->millivolts);
}

void print_netstat_record(struct netstat_record *record)
{
    printf("{\"time\":%" STAT_TIME_FMT
           ",\"type\":%d"
           ",\"tx_unicast_count\":%" PRIu32
           ",\"tx_mcast_count\":%" PRIu32
           ",\"tx_success\":%" PRIu32
           ",\"tx_failed\":%" PRIu32
           ",\"tx_bytes\":%" PRIu32
           ",\"rx_count\":%" PRIu32
           ",\"rx_bytes\":%" PRIu32 "}",
           record->time,
           record->type,
           record->tx_unicast_count,
           record->tx_mcast_count,
           record->tx_success,
           record->tx_failed,
           record->tx_bytes,
           record->rx_count,
           record->rx_bytes);
}

void print_capture_record(struct capture_record *record)
{
    printf("{\"time\":%" STAT_TIME_FMT
           ",\"event_type\":%d"
           ",\"packet_type\":%d"
           ",\"headers_len\":%" PRIu16
           ",\"payload_len\":%" PRIu16 "}",
           record->time,
           record->event_type,
           record->packet_type,
           record->headers_len,
           record->payload_len);
}

void print_latency_record(struct latency_record *record)
{
    printf("{\"time\":%" STAT_TIME_FMT
           ",\"type\":%d"
           ",\"round_trip_time\":%" STAT_TIME_FMT "}",
           record->time,
           record->type,
           record->round_trip_time);
}
