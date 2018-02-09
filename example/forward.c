#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netdb.h>
#include <err.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>

#include <dynamic.h>
#include <reactor.h>
#include <packet.h>

struct state
{
  packet reader;
  packet writer;
};

static int event(void *state, int type, void *data)
{
  struct state *s = state;
  packet_frame *f;

  switch (type)
    {
    case PACKET_EVENT_READ:
      f = data;
      packet_write(&s->writer, f);
      return REACTOR_OK;
    case PACKET_EVENT_ERROR:
      err(1, "packet event type %d", type);
    case PACKET_EVENT_WRITE:
    default:
      return REACTOR_OK;
    }
}

static void usage(void)
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s FROM TO\n", __progname);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  struct state state;
  int e;

  if (argc != 3)
    usage();

  (void) reactor_core_construct();
  e = packet_open(&state.reader, event, &state, PACKET_TYPE_READER, if_nametoindex(argv[1]), 2048, 1024 * 2048, 4);
  if (e == -1)
    err(1, "packet_open");

  e = packet_open(&state.writer, event, &state, PACKET_TYPE_WRITER, if_nametoindex(argv[2]), 2048, 1024 * 2048, 4);
  if (e == -1)
    err(1, "packet_open");

  (void) reactor_core_run();
  packet_close(&state.reader);
  packet_close(&state.writer);

  reactor_core_destruct();
}
