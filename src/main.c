#include "board_config.h"
#include "net/gnrc/nettype.h"
#include "shell.h"
#include "tsrb.h"
#include "ztimer.h"
#include "stdio.h"

#include "netstats.h"
#include "display.h"
#include "stats.h"
#include "pkt_capture.h"
#include "sendrecv.h"

#define ENABLE_DEBUG 0
#include "debug.h"

// Must be powers of two (limitation of tsrb)
#define MAX_BYTES_NETSTAT_RECORDS (2 << 12)
#define MAX_BYTES_POWER_RECORDS   (2 << 12)
#define MAX_BYTES_CAPTURE_RECORDS (2 << 10)
#define MAX_BYTES_LATENCY_RECORDS (2 << 8)

// Allocate statically, so that we don't need to use a memory allocator/linked list
static uint8_t netstat_buffer[MAX_BYTES_NETSTAT_RECORDS];
static uint8_t power_buffer[MAX_BYTES_POWER_RECORDS];
static uint8_t capture_buffer[MAX_BYTES_CAPTURE_RECORDS];
static uint8_t latency_buffer[MAX_BYTES_LATENCY_RECORDS];

// use thread safe buffers for inter-thread communication and data storage
static tsrb_t netstat_ringbuffer;
static tsrb_t power_ringbuffer;
static tsrb_t capture_ringbuffer;
static tsrb_t latency_ringbuffer;

// Keep the configuration in a global so we have the option to modify it in a shell command
static struct node_configuration config;

static void *_shell_loop(void *ctx)
{
    (void)ctx;
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
    return NULL;
}

int main(void)
{
    // Get the configuration for this board
    config = get_node_configuration();
    // Apply configuration
    gnrc_netif_t *radio = get_lora_netif();
    assert(radio != NULL);
    // No point continuing if we can't configure correctly
    assert(apply_node_configuration(radio, &config) == 0);

    tsrb_init(&netstat_ringbuffer, (unsigned char *)netstat_buffer, sizeof(netstat_buffer));
    tsrb_init(&power_ringbuffer, (unsigned char *)power_buffer, sizeof(power_buffer));
    tsrb_init(&capture_ringbuffer, (unsigned char *)capture_buffer, sizeof(capture_buffer));
    tsrb_init(&latency_ringbuffer, (unsigned char *)latency_buffer, sizeof(latency_buffer));

    init_stats_thread(&(struct stats_thread_args){ .power_tsrb = &power_ringbuffer, .netstat_tsrb = &netstat_ringbuffer });
    init_display_thread(&(struct display_thread_args){ .power_ringbuffer = &power_ringbuffer, .netstat_ringbuffer = &netstat_ringbuffer, .capture_ringbuffer = &capture_ringbuffer, .config = &config });
    init_pkt_capture_thread(&(struct pkt_capture_thread_args){ .capture_tsrb = &capture_ringbuffer });
    init_sendrecv_thread(&(struct sendrecv_thread_args){ .latency_tsrb = &latency_ringbuffer, .config = &config });

    (void)puts("Threads started, running shell\n");
    _shell_loop(NULL);

    return 0;
}

static int _buffer_state(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    unsigned power_count = tsrb_avail(&power_ringbuffer);
    unsigned netstat_count = tsrb_avail(&netstat_ringbuffer);
    unsigned capture_count = tsrb_avail(&capture_ringbuffer);
    unsigned latency_count = tsrb_avail(&latency_ringbuffer);

    printf("Netstat bytes/count: %u/%u, Power bytes/count: %u/%u, Capture bytes/count: %u/%u, Latency bytes/count: %u/%u\n",
           netstat_count, netstat_count / sizeof(struct netstat_record),
           power_count, power_count / sizeof(struct power_record),
           capture_count, capture_count / sizeof(struct capture_record),
           latency_count, latency_count / sizeof(struct latency_record));

    unsigned int current_time = ztimer_now(ZTIMER_MSEC);
    printf("Current time is %u\n", current_time);
    return 0;
}

// Define function that just checks that a void pointer has the right amount of data
// and passes along the argument (to be able to use prints interchageably)
#define GENERATE_VOID_CAST_FUNC(in_func, out_func, struct_name) \
    static void out_func(void *data, size_t len)                \
    {                                                           \
        assert(len == sizeof(struct_name));                     \
        in_func(data);                                          \
    }

GENERATE_VOID_CAST_FUNC(print_power_record, print_power_record_data, struct power_record)
GENERATE_VOID_CAST_FUNC(print_netstat_record, print_netstat_record_data, struct netstat_record)
GENERATE_VOID_CAST_FUNC(print_capture_record, print_capture_record_data, struct capture_record)
GENERATE_VOID_CAST_FUNC(print_latency_record, print_latency_record_data, struct latency_record)

static int _dump_buffer(int argc, char **argv)
{
    // Eventually, could pass arguments to filter results
    (void)argc;
    (void)argv;

    puts("{");
    printf("\"node_id\":%u,", CONFIG_ID);

    printf("\"latency_records\":");
    print_record_json_array(&latency_ringbuffer, sizeof(struct latency_record), print_latency_record_data);
    puts(",");

    puts("\"capture_records\":");
    print_record_json_array(&capture_ringbuffer, sizeof(struct capture_record), print_capture_record_data);
    puts(",");

    puts("\"power_records\":");
    print_record_json_array(&power_ringbuffer, sizeof(struct power_record), print_power_record_data);
    puts(",");

    puts("\"netstat_records\":");
    print_record_json_array(&netstat_ringbuffer, sizeof(struct netstat_record), print_netstat_record_data);

    puts("}\n");

    return 0;
}

SHELL_COMMAND(buffer_state, "Show the state of the data collection buffers", _buffer_state);
SHELL_COMMAND(dump_buffer, "Print all the data that has been accumulated in the buffers and clear it", _dump_buffer);
