#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netdb.h>
#include <time.h>
#include <err.h>

#include <dynamic.h>
#include <reactor.h>
#include <packet.h>

struct state
{
  packet        writer;
};

static int event(void *state, int type, void *data)
{
  struct state *s = state;

  (void) s;
  (void) data;
  switch (type)
    {
    case PACKET_EVENT_WRITE:
      return REACTOR_OK;
    default:
      err(1, "event");
    }
}

static void usage(void)
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s IP\n", __progname);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  struct state state;
  int e;

  if (argc != 2)
    usage();

  (void) reactor_core_construct();
  e = packet_open(&state.writer, event, &state, PACKET_TYPE_WRITER, argv[1], 2048, 128 * 2048, 4);
  if (e == -1)
    err(1, "packet_open");

  (void) reactor_core_run();
  reactor_core_destruct();
}
