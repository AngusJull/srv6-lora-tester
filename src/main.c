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
#include "stdlib.h"
#include "srv6.h"

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

static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

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

    // initialize message queue for the main thread
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    gnrc_udp_init();

    // init udp server for dest
    srv6_udp_server_start();

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

static int _set_config(int argc, char **argv)
{
    if (argc != 4) {
        puts("Set configuration for this board\n");
        puts("Usage: set_config <config_id> <toplogy_id> <use_srv6>");
        return 1;
    }

    struct saved_config new_config = { 0 };
    new_config.config_id = atoi(argv[1]);
    new_config.chosen_topology = atoi(argv[2]);
    new_config.use_srv6 = atoi(argv[3]);

    set_saved_configuration(new_config);

    struct saved_config updated = get_saved_configuration();
    printf("Updated fields are %u %u %u\n", updated.config_id, updated.chosen_topology, updated.use_srv6);

    return 0;
}

static int _srv6_ping(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: srv6_ping <destination> <segment1> [segment2 ...]\n");
        return 1;
    }

    ipv6_addr_t dest;
    if (ipv6_addr_from_str(&dest, argv[1]) == NULL) {
        printf("Invalid destination address\n");
        return 1;
    }

    int num_segments = argc - 1; // includes destination as last segment
    if (num_segments < 1 || num_segments > 255) {
        printf("Invalid number of segments (1-255)\n");
        return 1;
    }

    const char *msg = "hello world";
    gnrc_pktsnip_t *pkt = srv6_build_pkt(&dest, argv, num_segments, CLIENT_PORT, SERVER_PORT, msg, sizeof(msg));

    debug_print_snip_chain("before sending packet", pkt);
    // send packet to IPv6 layer for transmission
    if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_IPV6, GNRC_NETREG_DEMUX_CTX_ALL, pkt)) {
        printf("Failed to send packet\n");
        gnrc_pktbuf_release(pkt);
        return 1;
    }

    printf("SRv6 packet sent to %s with %d segments\n", argv[1], num_segments);
    return 0;
}

SHELL_COMMAND(srv6_ping, "Send UDP message with SRv6 SRH", _srv6_ping);
SHELL_COMMAND(buffer_state, "Show the state of the data collection buffers", _buffer_state);
SHELL_COMMAND(dump_buffer, "Print all the data that has been accumulated in the buffers and clear it", _dump_buffer);
SHELL_COMMAND(set_config, "Set the configuration information for this board", _set_config)
