#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <err.h>

#include <asm/types.h>
#include <libnetlink.h>
#include <linux/netlink.h>
#include <linux/in_route.h>


#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <arpa/inet.h>

#include <dynamic.h>
#include <reactor.h>
#include <packet.h>

#include "packet_route.h"

static void usage(void)
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s node\n", __progname);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  //struct state state;
  packet_route route;
  int e;

  if (argc != 2)
    usage();

  (void) reactor_core_construct();

  e = packet_route_construct(&route, argv[1]);
  if (e == -1)
    err(1, "packet_route_construct");
  packet_route_debug(&route, stderr);

  //get_route(argv[2]);


  /*
  e = packet_open(&state.writer, event, &state, PACKET_TYPE_WRITER, argv[1], 2048, 128 * 2048, 4);
  if (e == -1)
    err(1, "packet_open");

  ping(&state.writer, argv[1], argv[2]);
  */
  //(void) reactor_core_run();
  reactor_core_destruct();
}
