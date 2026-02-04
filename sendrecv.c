// Create a  thread with the same priority as others
//
// Thread should read configuration from its arguments, then send a packet using srv6 if specified.
// Thread should then wait for a response. Upon receiving it, it puts some info into another TSRB and we can get that later

#include "stats.h"
#include "ztimer.h"

#include "sendrecv.h"

#define ENABLE_DEBUG 0
#include "debug.h"

// Threads that need to run whenever availabile
#define THREAD_PRIORITY_MED (THREAD_PRIORITY_MAIN - 2)
// Stack for the stats thread
static char _stack[THREAD_STACKSIZE_MEDIUM];

void send(void)
{
    ztimer_sleep(ZTIMER_MSEC, 1000);
}

void recv(void)
{
    ztimer_sleep(ZTIMER_MSEC, 1000);
}

void *_sender_loop(void *ctx)
{
    struct sendrecv_thread_args *args = ctx;

    while (1) {
        ztimer_now_t start = ztimer_now(ZTIMER_MSEC);
        send();
        recv();
        // Getting our time from the ztimer instead of the packet means our time will be consistent with
        // the rest of the times we are measuring
        ztimer_now_t end = ztimer_now(ZTIMER_MSEC);

        ztimer_now_t round_trip = start - end;
        struct latency_record latency = { .time = end, .round_trip_time = round_trip };
        add_record(args->latency_tsrb, (unsigned char *)&latency, sizeof(latency));
    }
    return NULL;
}

void *_receiver_loop(void *ctx)
{
    struct sendrecv_thread_args *args = ctx;
    (void)args;

    while (1) {
        recv();
        send();
    }
    return NULL;
}

void init_sendrecv_thread(struct sendrecv_thread_args *args)
{
    switch (args->traffic_config->role) {
    case NODE_ROLE_SENDER:
        thread_create(_stack, sizeof(_stack), THREAD_PRIORITY_MED, 0, _sender_loop, args, "sender");
        break;
    case NODE_ROLE_RECIEVER:
        thread_create(_stack, sizeof(_stack), THREAD_PRIORITY_MED, 0, _receiver_loop, args, "receiver");
        break;
    default:
        // If this node is NODE_ROLE_FORWARD_ONLY, don't start any threads
        break;
    }
}
