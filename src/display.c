#include "thread.h"
#include "u8g2.h"
#include "u8x8_riotos.h"
#include "ztimer.h"
#include "stdio.h"
#include "string.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#include "configs/config_common.h"
#include "records.h"
#include "display.h"

#define TIME_BETWEEN_DRAW_MS             50
#define MAX_TIME_SINCE_CAPTURED_MS       500

#define DISPLAY_DEACTIVATE_PIN           GPIO_PIN(0, 36) // OLED Power Control
#define DISPLAY_RST_PIN                  GPIO_PIN(0, 21) // OLED Reset

#define DISPLAY_I2C                      0    // I2C Line, could be made into a
#define DISPLAY_I2C_ADDR                 0x3c // I2C Address

// Maximum character widths for integers
#define MAX_STRLEN                       30
#define MAX_BAT_WIDTH                    4 // Maximum value for battery, to prevent using more than four chars
#define MAX_ID_WIDTH                     2
#define MAX_BYTE_WIDTH                   5
#define MAX_COUNT_WIDTH                  5

// Routing notification settings for l3 packets
#define ROUTE_NOTIF_L3_COMPLETE_SEGMENTS 2 // The max number of arrow segments or movements to make when displaying a routing notification
#define ROUTE_NOTIF_L3_TIME_PER_SEGMENT  50
#define ROUTE_NOTIF_L3_IN_PROGRESS_TIME  (ROUTE_NOTIF_L3_COMPLETE_SEGMENTS * ROUTE_NOTIF_L3_TIME_PER_SEGMENT)

// For l2 packest
#define ROUTE_NOTIF_L2_COMPLETE_SEGMENTS 3 // The max number of arrow segments or movements to make when displaying a routing notification
#define ROUTE_NOTIF_L2_TIME_PER_SEGMENT  50
#define ROUTE_NOTIF_L2_IN_PROGRESS_TIME  (ROUTE_NOTIF_L2_COMPLETE_SEGMENTS * ROUTE_NOTIF_L2_TIME_PER_SEGMENT)

#define ROUTE_NOTIF_CLEAR_AFTER_MS       3500

// Allow integer widths to be used in string formatting
#define _STRINGIFY(x)                    #x
// Force expansion of the enclosed macro
#define STR(macro)                       _STRINGIFY(macro)

#define DEFAULT_PAD_X                    3
#define DEFAULT_PAD_Y                    3

#define DISPLAY_THREAD_PRIORITY          (THREAD_PRIORITY_MAIN - 1)
static char _stack[THREAD_STACKSIZE_MEDIUM];

// For creating formatted strings for display (and using the stored string immediately)
static char text_buffer[MAX_STRLEN];

static u8g2_t u8g2;
static u8x8_riotos_t user_data = {
    .device_index = DISPLAY_I2C,
    .pin_cs = GPIO_UNDEF,
    .pin_dc = GPIO_UNDEF,
    .pin_reset = DISPLAY_RST_PIN,
};

#define LINE_SPACE ((unsigned char)u8g2_GetMaxCharHeight(&u8g2) + DEFAULT_PAD_Y)

static void next_line(unsigned int *cursor_x, unsigned int *cursor_y)
{
    *cursor_x = 0;
    *cursor_y += LINE_SPACE;
}

static inline unsigned int clamp_width(unsigned int value, unsigned int width)
{
    unsigned int max = 0;
    for (unsigned int i = 0; i < width; i++) {
        max *= 10; //NOLINT
        max += 9;  //NOLINT
    }

    if (value > max) {
        return max;
    }
    return value;
}

struct routing_notif_l3_substate {
    bool display;
    unsigned int progress; // Progress of the animation, between 0 and 1000
    unsigned int segs;
    int dest;
};

struct routing_notif_l2_substate {
    bool display;
    unsigned int progress;
    // For now, no extra information for L2. Just show progress bar
};

struct routing_notif_state {
    struct routing_notif_l3_substate l3_in;
    struct routing_notif_l3_substate l3_out;
    struct routing_notif_l2_substate l2_in;
    struct routing_notif_l2_substate l2_out;
};

struct process_routing_state_inner_ctx {
    stat_time_t now;
    bool l3_in_complete;
    bool l3_out_complete;
    bool l2_in_complete;
    bool l2_out_complete;
    struct routing_notif_state *routing;
};

static int process_routing_l3_substate(struct routing_notif_l3_substate *substate, struct capture_record *record, stat_time_t now)
{
    if (record->time < now) {
        stat_time_t diff = now - record->time;
        unsigned int progress = diff;
        if (progress > ROUTE_NOTIF_L3_IN_PROGRESS_TIME) {
            progress = ROUTE_NOTIF_L3_IN_PROGRESS_TIME;
        }
        // Don't show when too old
        substate->display = diff < ROUTE_NOTIF_CLEAR_AFTER_MS;
        substate->progress = progress;
        substate->dest = record->dest_id; //NOLINT. Prevent linter from suggesting we turn into unsigned char first to preserve bits
        substate->segs = record->segments_left;

        return 1;
    }
    return 0;
}

static int process_routing_l2_substate(struct routing_notif_l2_substate *substate, struct capture_record *record, stat_time_t now)
{
    if (record->time < now) {
        stat_time_t diff = now - record->time;
        unsigned int progress = diff;
        if (progress > ROUTE_NOTIF_L2_IN_PROGRESS_TIME) {
            progress = ROUTE_NOTIF_L2_IN_PROGRESS_TIME;
        }
        substate->display = diff < ROUTE_NOTIF_CLEAR_AFTER_MS;
        substate->progress = progress;

        return 1;
    }
    return 0;
}

static int process_routing_state_inner(uint8_t *record_data, size_t record_size, void *ctx)
{
    struct process_routing_state_inner_ctx *context = ctx;

    // Make sure we got the right type of data
    assert(record_size == sizeof(struct capture_record));
    struct capture_record record;
    memcpy(&record, record_data, record_size);

    // Only interested in L3 packets
    switch (record.packet_type) {
    case CAPTURE_PACKET_TYPE_SRV6:
        // Fall-through
    case CAPTURE_PACKET_TYPE_IPV6:
        // Check what type of record this is, make sure we don't have it already
        if (!context->l3_out_complete && record.event_type == CAPTURE_EVENT_TYPE_SEND) {
            context->l3_out_complete = process_routing_l3_substate(&context->routing->l3_out, &record, context->now);
        }
        else if (!context->l3_in_complete && record.event_type == CAPTURE_EVENT_TYPE_RECV) {
            context->l3_in_complete = process_routing_l3_substate(&context->routing->l3_in, &record, context->now);
        }
        break;
    case CAPTURE_PACKET_TYPE_SIXLOWPAN:
        if (!context->l2_out_complete && record.event_type == CAPTURE_EVENT_TYPE_SEND) {
            context->l2_out_complete = process_routing_l2_substate(&context->routing->l2_out, &record, context->now);
        }
        else if (!context->l2_in_complete && record.event_type == CAPTURE_EVENT_TYPE_RECV) {
            context->l2_in_complete = process_routing_l2_substate(&context->routing->l2_in, &record, context->now);
        }
        break;
    default:
        break;
    }

    return context->l3_in_complete && context->l3_out_complete && context->l2_in_complete && context->l2_out_complete;
}

static void process_routing_state(struct routing_notif_state *routing, struct record_list *capture_records)
{
    routing->l3_in.display = false;
    routing->l3_out.display = false;
    routing->l2_in.display = false;
    routing->l2_out.display = false;

    struct process_routing_state_inner_ctx context = {
        .now = ztimer_now(ZTIMER_MSEC),
        .l3_in_complete = false,
        .l3_out_complete = false,
        .l2_in_complete = false,
        .l2_out_complete = false,
        .routing = routing,
    };
    record_list_iter(capture_records, process_routing_state_inner, &context);
}

static unsigned int draw_route_notif_arrow(bool display, unsigned int progress, unsigned int progress_per_seg, unsigned int total_segs, unsigned int cursor_x, unsigned int cursor_y)
{
    // Draw routing arrows
    unsigned int chars_used = 0;
    unsigned int offset_x = 0;
    if (display) {
        chars_used += progress / progress_per_seg;
        for (unsigned int i = 0; i < chars_used; i++) {
            offset_x += u8g2_DrawStr(&u8g2, cursor_x + offset_x, cursor_y, "=");
        }
        chars_used += 1;
        offset_x += u8g2_DrawStr(&u8g2, cursor_x + offset_x, cursor_y, ">");
    }
    // Fill rest of bar in with empty spaces (account for arrow head and arrow segments)
    for (unsigned int i = 0; i < ((total_segs + 1) - chars_used); i++) {
        offset_x += u8g2_DrawStr(&u8g2, cursor_x + offset_x, cursor_y, " ");
    }

    return offset_x;
}

static void draw_route_notif_info(bool display, int dest, unsigned int segs, unsigned int cursor_x, unsigned int cursor_y)
{
    if (display) {
        snprintf(text_buffer, sizeof(text_buffer), "DEST %d", dest);
        u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer);
        cursor_y += LINE_SPACE;
        snprintf(text_buffer, sizeof(text_buffer), "SEGS %u", segs);
        u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer);
    }
    else {
        u8g2_DrawStr(&u8g2, cursor_x, cursor_y, "DEST -");
        cursor_y += LINE_SPACE;
        u8g2_DrawStr(&u8g2, cursor_x, cursor_y, "SEGS -");
    }
}

static void draw_route_notif(struct routing_notif_l3_substate *l3_substate, struct routing_notif_l2_substate *l2_substate, unsigned int cursor_x, unsigned int cursor_y)
{
    unsigned int arrow_row_x = cursor_x + u8g2_DrawStr(&u8g2, cursor_x, cursor_y, "[");
    arrow_row_x += draw_route_notif_arrow(l2_substate->display, l2_substate->progress,
                                          ROUTE_NOTIF_L2_TIME_PER_SEGMENT,
                                          ROUTE_NOTIF_L2_COMPLETE_SEGMENTS,
                                          arrow_row_x,
                                          cursor_y);
    arrow_row_x += u8g2_DrawStr(&u8g2, arrow_row_x, cursor_y, "|");
    // Draw the two arrows one after another
    arrow_row_x += draw_route_notif_arrow(l3_substate->display, l3_substate->progress,
                                          ROUTE_NOTIF_L3_TIME_PER_SEGMENT,
                                          ROUTE_NOTIF_L3_COMPLETE_SEGMENTS,
                                          arrow_row_x,
                                          cursor_y);
    u8g2_DrawStr(&u8g2, arrow_row_x, cursor_y, "]");

    cursor_y += LINE_SPACE;
    draw_route_notif_info(l3_substate->display, l3_substate->dest, l3_substate->segs, cursor_x, cursor_y);
}

// Draw to the display, providng all parameters that will be shown on the display
static void draw_display(struct routing_notif_state *routing, struct stats_record *stats, struct node_configuration *config)
{
    // Use a monospaced (for readability and predictability in size of output) font with restricted character set (less overhead from unused chars)
    u8g2_SetFont(&u8g2, u8g2_font_6x10_mr);
    const unsigned int font_height = (unsigned char)u8g2_GetMaxCharHeight(&u8g2);

    // Loop for transferring data to the display incrementally, completes when all data transferred
    u8g2_FirstPage(&u8g2);
    do {
        unsigned int cursor_x = 0;
        // u8g2 draws downwards, so set cursor to after the first line on the display
        unsigned int cursor_y = font_height;

        // Draw battery

        snprintf(text_buffer, sizeof(text_buffer), "%." STR(MAX_BAT_WIDTH) "dmV", clamp_width(stats->millivolts, MAX_BAT_WIDTH));
        cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer) + DEFAULT_PAD_X;

        // Draw configuration

        snprintf(text_buffer, sizeof(text_buffer), "ID:%." STR(MAX_ID_WIDTH) "d", clamp_width(config->this_id, MAX_ID_WIDTH));
        cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer) + DEFAULT_PAD_X;
        snprintf(text_buffer, sizeof(text_buffer), "TOPO:%." STR(MAX_ID_WIDTH) "d", clamp_width(config->topology_id, MAX_ID_WIDTH));
        cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer) + DEFAULT_PAD_X;

        next_line(&cursor_x, &cursor_y);

        // Draw routing notification

        unsigned int width = u8g2_GetDisplayWidth(&u8g2);
        unsigned int half_width = width / 2;

        u8g2_DrawStr(&u8g2, 0, cursor_y, "IN");
        draw_route_notif(&routing->l3_in, &routing->l2_in, 0, cursor_y + LINE_SPACE);

        u8g2_DrawStr(&u8g2, half_width, cursor_y, "OUT");
        draw_route_notif(&routing->l3_out, &routing->l2_out, half_width, cursor_y + LINE_SPACE);
    } while (u8g2_NextPage(&u8g2));
}

static void *_display_loop(void *ctx)
{
    struct display_thread_args *args = ctx;

    struct stats_record stats = { 0 };

    while (1) {
        // By not handling empty case - either print zeroed capture or keep whatever we had last
        record_list_first(args->stats_list, (uint8_t *)&stats, sizeof(stats));

        struct routing_notif_state routing = { 0 };
        process_routing_state(&routing, args->display_capture_list);

        draw_display(&routing, &stats, args->config);
        // 4Hz refresh rate to not use up too much battery life, hopefully
        ztimer_sleep(ZTIMER_MSEC, TIME_BETWEEN_DRAW_MS);
    }
    return NULL;
}

// Prepare the display for drawing
int init_display_thread(struct display_thread_args *args)
{
    gpio_init(DISPLAY_DEACTIVATE_PIN, GPIO_OUT);
    gpio_clear(DISPLAY_DEACTIVATE_PIN);

    u8g2_Setup_ssd1306_i2c_128x64_noname_1(&u8g2, U8G2_R0, u8x8_byte_hw_i2c_riotos, u8x8_gpio_and_delay_riotos);

    u8g2_SetUserPtr(&u8g2, &user_data);
    u8g2_SetI2CAddress(&u8g2, DISPLAY_I2C_ADDR);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    thread_create(_stack, sizeof(_stack), DISPLAY_THREAD_PRIORITY, 0, _display_loop, args, "display");
    return 0;
}
