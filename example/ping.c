#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <err.h>

#include <libnetlink.h>
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
  packet_route route;
  packet       writer;
  packet_frame frame;
};

static void usage(void)
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s node\n", __progname);
  exit(EXIT_FAILURE);
}

static int event(void *state, int type, void *data)
{
  printf("%p %d %p\n", state, type, data);
  return REACTOR_OK;
}

void ping(struct state *s)
{
  packet_frame f = {0};
  struct ethhdr h;
  struct iphdr ip;
  struct icmphdr icmp;
  char data[8] = {0};

  packet_route_ether(&s->route, &h, ETH_P_IP);
  packet_route_ip(&s->route, &ip, sizeof icmp + sizeof data, IPPROTO_ICMP);

  packet_header_icmp(&icmp, ICMP_ECHO, 0, data, sizeof data);

  packet_frame_push(&f, PACKET_LAYER_TYPE_ETHER, &h, sizeof h);
  packet_frame_push(&f, PACKET_LAYER_TYPE_IP, &ip, sizeof ip);
  packet_frame_push(&f, PACKET_LAYER_TYPE_ICMP, &icmp, sizeof icmp);
  packet_frame_push(&f, PACKET_LAYER_TYPE_DATA, data, sizeof data);

  packet_write(&s->writer, &f);

  //packet_route_write_udp(&s->route, &s->writer, 5004, 5004, data, sizeof data);
}


int main(int argc, char **argv)
{
  struct state state;
  int e;

  if (argc != 2)
    usage();

  (void) reactor_core_construct();

  e = packet_route_construct(&state.route, argv[1]);
  if (e == -1)
    err(1, "packet_route_construct");

  e = packet_open(&state.writer, event, &state, PACKET_TYPE_WRITER, state.route.index, 2048, 128 * 2048, 4);
  if (e == -1)
    err(1, "packet_open");

  ping(&state);

  (void) reactor_core_run();
  reactor_core_destruct();
}
