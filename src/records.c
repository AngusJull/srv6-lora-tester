#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#include "records.h"

#define RECORD_LIST_INIT_CAPACITY 4

// Resize the list, but don't lock the mutex. Mutex MUST be locked already
// Doesn't look like RIOT checks owner of mutex when locking
static int _record_list_resize(struct record_list *list, unsigned int capacity)
{
    // If capacity is zero, elements must already be free
    if (capacity == 0 && list->capacity == 0) {
        return 0;
    }

    uint8_t *new_elements = realloc(list->elements, list->record_size * capacity);
    if (new_elements) {
        list->elements = new_elements;
        list->capacity = capacity;
        DEBUG("Resized list to %u elements, %u bytes\n", capacity, capacity * list->record_size);
        return 0;
    }
    printf("Failed to allocate new memory!\n");
    return 1;
}

// Initalize a new list with no elements
void record_list_init(struct record_list *list, unsigned int record_size)
{
    mutex_init(&list->mutex);
    mutex_lock(&list->mutex);

    list->record_size = record_size;
    list->capacity = 0;
    list->len = 0;
    _record_list_resize(list, RECORD_LIST_INIT_CAPACITY);

    mutex_unlock(&list->mutex);
}

// Resize the list to store capacity elements
int record_list_resize(struct record_list *list, unsigned int capacity)
{
    mutex_lock(&list->mutex);

    int ret = _record_list_resize(list, capacity);

    mutex_unlock(&list->mutex);

    return ret;
}

// Add an element to the front of the list by allocating a new element and copying some data into it
// Return 0 if the element was added, or 1 otherwise
int record_list_insert(struct record_list *list, uint8_t *record, size_t record_size)
{
    assert(record_size == list->record_size);

    int ret = 0;
    mutex_lock(&list->mutex);

    // If we end up out of space, allocate more
    if (list->len >= list->capacity && _record_list_resize(list, list->capacity * 2)) {
        ret = 1;
    }
    else {
        // Actually put the element on the end, but consistently use the list as if it goes
        // on the front so just use that terminology
        memcpy(&list->elements[list->len * list->record_size], record, record_size);
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
        if (func(&list->elements[i * list->record_size], list->record_size, ctx)) {
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
        memcpy(record, &list->elements[(list->len - 1) * list->record_size], record_size);
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
    _record_list_resize(list, RECORD_LIST_INIT_CAPACITY);

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
        puts(",");
    }
    args->index++;
    args->print_func(data, data_len);

    return 0;
}

void print_record_list_json_array(struct record_list *list, void (*print_func)(void *, size_t))
{
    puts("[");
    struct print_record_list_json_array_inner_ctx args = { .index = 0, .print_func = print_func };
    record_list_iter(list, _print_record_list_json_array_inner, &args);
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
           ",\"tx_bytes\":%" PRIu32
           ",\"rx_count\":%" PRIu32
           ",\"rx_bytes\":%" PRIu32 "}",
           record->time,
           record->type,
           record->tx_unicast_count,
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
