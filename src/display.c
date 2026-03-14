#include "thread.h"
#include "u8g2.h"
#include "u8x8_riotos.h"
#include "ztimer.h"
#include "stdio.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#include "configs/config_common.h"
#include "records.h"
#include "display.h"

#define TIME_BETWEEN_DRAW_MS       200
#define MAX_TIME_SINCE_CAPTURED_MS 2000

#define DISPLAY_DEACTIVATE_PIN     GPIO_PIN(0, 36) // OLED Power Control
#define DISPLAY_RST_PIN            GPIO_PIN(0, 21) // OLED Reset

#define DISPLAY_I2C                0    // I2C Line, could be made into a
#define DISPLAY_I2C_ADDR           0x3c // I2C Address

// Maximum character widths for integers
#define MAX_STRLEN                 30
#define MAX_BAT_WIDTH              4 // Maximum value for battery, to prevent using more than four chars
#define MAX_ID_WIDTH               2
#define MAX_BYTE_WIDTH             5
#define MAX_COUNT_WIDTH            5

// Allow integer widths to be used in string formatting
#define _STRINGIFY(x)              #x
// Force expansion of the enclosed macro
#define STR(macro)                 _STRINGIFY(macro)

#define DEFAULT_PAD_X              3
#define DEFAULT_PAD_Y              3

#define DISPLAY_THREAD_PRIORITY    (THREAD_PRIORITY_MAIN - 1)
static char _stack[THREAD_STACKSIZE_MEDIUM];

static u8g2_t u8g2;
static u8x8_riotos_t user_data = {
    .device_index = DISPLAY_I2C,
    .pin_cs = GPIO_UNDEF,
    .pin_dc = GPIO_UNDEF,
    .pin_reset = DISPLAY_RST_PIN,
};

static void next_line(unsigned int *cursor_x, unsigned int *cursor_y, unsigned int font_height, unsigned int pad_y)
{
    *cursor_x = 0;
    *cursor_y += font_height + pad_y;
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

// Draw to the display, providng all parameters that will be shown on the display
void draw_display(int display_route_notif, struct node_configuration *config, struct stats_record *stats)
{
    static char text_buffer[MAX_STRLEN];

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

        // Draw the configuration

        snprintf(text_buffer, sizeof(text_buffer), "ID:%." STR(MAX_ID_WIDTH) "d", clamp_width(config->this_id, MAX_ID_WIDTH));
        cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer) + DEFAULT_PAD_X;
        snprintf(text_buffer, sizeof(text_buffer), "TOP:%." STR(MAX_ID_WIDTH) "d", clamp_width(config->topology_id, MAX_ID_WIDTH));
        cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer) + DEFAULT_PAD_X;
        snprintf(text_buffer, sizeof(text_buffer), "SR:%." STR(MAX_ID_WIDTH) "d", clamp_width(config->use_srv6, MAX_ID_WIDTH));
        cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer) + DEFAULT_PAD_X;
        snprintf(text_buffer, sizeof(text_buffer), "TP:%." STR(MAX_ID_WIDTH) "d", clamp_width(config->throughput_test, MAX_ID_WIDTH));
        cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer) + DEFAULT_PAD_X;

        next_line(&cursor_x, &cursor_y, font_height, DEFAULT_PAD_Y);

        // Draw routing notification

        if (display_route_notif) {
            cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, "|ROUTED|");
        }
        next_line(&cursor_x, &cursor_y, font_height, DEFAULT_PAD_Y);

        // Draw interface stats

        cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, "STATS");
        next_line(&cursor_x, &cursor_y, font_height, DEFAULT_PAD_Y);

        snprintf(text_buffer, sizeof(text_buffer), "TX %u RX %u",
                 clamp_width(stats->l3.tx_unicast_count, MAX_COUNT_WIDTH),
                 clamp_width(stats->l3.rx_count, MAX_COUNT_WIDTH));
        cursor_x += u8g2_DrawStr(&u8g2, cursor_x, cursor_y, text_buffer);
        next_line(&cursor_x, &cursor_y, font_height, DEFAULT_PAD_Y);

        // Look into special fonts with icons for better UI elements
    } while (u8g2_NextPage(&u8g2));
}

static void *_display_loop(void *ctx)
{
    struct display_thread_args *args = ctx;

    struct stats_record stats = { 0 };
    struct capture_record capture = { 0 };

    while (1) {
        // By not handling empty case - either print zeroed capture or keep whatever we had last
        record_list_first(args->stats_list, (uint8_t *)&stats, sizeof(stats));
        record_list_first(args->capture_list, (uint8_t *)&capture, sizeof(capture));

        int display_route_notif = 0;
        ztimer_now_t time = ztimer_now(ZTIMER_MSEC);
        if (time - capture.time < MAX_TIME_SINCE_CAPTURED_MS) {
            display_route_notif = 1;
        }

        draw_display(display_route_notif, args->config, &stats);
        // 4Hz refresh rate to not use up too much battery life, hopefully
        DEBUG("Display sleeping\n");
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
