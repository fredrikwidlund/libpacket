#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netdb.h>
#include <time.h>
#include <err.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <dynamic.h>
#include <reactor.h>
#include <packet.h>

struct state
{
  reactor_timer timer;
  packet        writer;
  size_t        out;
  uint64_t      t0;
};

static uint64_t ntime(void)
{
  struct timespec ts;

  (void) clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return ((uint64_t) ts.tv_sec * 1000000000) + ((uint64_t) ts.tv_nsec);
}

static void flood(struct state *s)
{
  char data[1024] = {0};
  packet_frame frame;

  packet_frame_construct(&frame);
  packet_frame_push(&frame, PACKET_LAYER_TYPE_DATA, data, sizeof data);
  while (!packet_blocked(&s->writer))
    {
      packet_write(&s->writer, &frame);
      s->out ++;
    }
}

static int timer(void *state, int type, void *data)
{
  struct state *s = state;
  uint64_t t;

  (void) data;
  if (type != REACTOR_TIMER_EVENT_CALL)
    err(1, "timer");

  t = ntime();
  (void) t;

  (void) fprintf(stderr, "out: %lu pps\n", s->out);
  s->out = 0;
  return REACTOR_OK;
}

static int event(void *state, int type, void *data)
{
  (void) data;
  switch (type)
    {
    case PACKET_EVENT_WRITE:
      flood(state);
      return REACTOR_OK;
    default:
      err(1, "event");
    }
}

static void usage(void)
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s INTERFACE\n", __progname);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  struct state state;
  int e;

  if (argc != 2)
    usage();

  state.t0 = ntime();
  (void) reactor_core_construct();
  (void) reactor_timer_open(&state.timer, timer, &state, 1000000000, 1000000000);
  e = packet_open(&state.writer, event, &state, PACKET_TYPE_WRITER, if_nametoindex(argv[1]), 2048, 128 * 2048, 4);
  if (e == -1)
    err(1, "packet_open");

  flood(&state);
  (void) reactor_core_run();
  reactor_core_destruct();
}

