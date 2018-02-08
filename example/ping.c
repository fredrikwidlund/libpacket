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
#include <linux/ip.h>
#include <linux/icmp.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <arpa/inet.h>

#include <dynamic.h>
#include <reactor.h>
#include <packet.h>

#include "packet_route.h"

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

static inline uint32_t checksum_add(uint32_t sum, void *data, size_t size)
{
  size_t i;

  for (i = 0; i < size / 2; i ++)
    sum += ((uint16_t *) data)[i];

  if (size % 2)
    sum += ((uint8_t *) data)[size - 1];

  return sum;
}

static inline uint16_t checksum_final(uint32_t sum)
{
  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);

  return ~sum;
}

void packet_header_ether(struct ethhdr *h, uint8_t *src, uint8_t *dst, uint16_t proto)
{
  memcpy(h->h_dest, dst, sizeof h->h_dest);
  memcpy(h->h_source, src, sizeof h->h_source);
  h->h_proto = htons(proto);
}

void packet_header_ip(struct iphdr *ip, size_t size, int protocol, uint32_t src, uint32_t dst)
{
  *ip = (struct iphdr) {
    .version = 4,
    .ihl = sizeof(struct iphdr) >> 2,
    .tot_len = htons(sizeof(struct iphdr) + size),
    .ttl = IPDEFTTL,
    .protocol = protocol,
    .saddr = src,
    .daddr = dst
    };
  ip->check = checksum_final(checksum_add(0, ip, sizeof(struct iphdr)));
}

void packet_header_icmp(struct icmphdr *icmp, int type, int code, void *data, size_t size)
{
  *icmp = (struct icmphdr) {
    .type = type,
    .code = code
  };
  icmp->checksum = checksum_final(checksum_add(checksum_add(0, icmp, sizeof *icmp), data, size));
}

void packet_route_ether(packet_route *r, struct ethhdr *h, uint16_t proto)
{
  packet_header_ether(h, r->ether_src, r->ether_dst, proto);
}

void packet_route_ip(packet_route *r, struct iphdr *ip, size_t size, int protocol)
{
  packet_header_ip(ip, size, protocol, r->ip_src, r->ip_dst);
}

void ping(struct state *s)
{
  packet_frame f = {0};
  struct ethhdr h;
  struct iphdr ip;
  struct icmphdr icmp;
  char data[8] = {0};

  packet_route_ether(&s->route, &h, ETH_P_IP);
  packet_frame_push(&f, PACKET_LAYER_TYPE_ETHER, &h, sizeof h);

  packet_route_ip(&s->route, &ip, sizeof icmp + sizeof data, IPPROTO_ICMP);
  packet_frame_push(&f, PACKET_LAYER_TYPE_IP, &ip, sizeof ip);

  packet_header_icmp(&icmp, ICMP_ECHO, 0, data, sizeof data);
  packet_frame_push(&f, PACKET_LAYER_TYPE_ICMP, &icmp, sizeof icmp);

  packet_frame_push(&f, PACKET_LAYER_TYPE_DATA, data, sizeof data);

  packet_write(&s->writer, &f);
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
