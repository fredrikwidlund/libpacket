#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/epoll.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/udp.h>
#include <linux/tcp.h>

#include "packet_frame.h"

static void *packet_offset(void *data, size_t size)
{
  return (uint8_t *) data + size;
}

static packet_layer *packet_frame_top(packet_frame *f)
{
  return &f->layer[f->layers - 1];
}

static void packet_frame_icmp(packet_frame *f)
{
  packet_layer *l0 = &f->layer[f->layers - 1], *l1 = &f->layer[f->layers];
  int s0 = sizeof *l0->icmp, s1 = l0->size - s0;

  if (f->layers == PACKET_LAYERS_MAX || s0 > l0->size)
    return;
  f->layers ++;
  *l0 = (packet_layer) {.type = PACKET_LAYER_TYPE_ICMP, .data = l0->data, .size = s0};
  *l1 = (packet_layer) {.type = PACKET_LAYER_TYPE_DATA, .data = packet_offset(l0->data, s0), .size = s1};
}

static void packet_frame_tcp(packet_frame *f)
{
  packet_layer *l0 = &f->layer[f->layers - 1], *l1 = &f->layer[f->layers];
  int s0 = l0->tcp->doff << 2, s1 = l0->size - s0;

  if (f->layers == PACKET_LAYERS_MAX || s0 > l0->size)
    return;
  f->layers ++;
  *l0 = (packet_layer) {.type = PACKET_LAYER_TYPE_TCP, .data = l0->data, .size = s0};
  *l1 = (packet_layer) {.type = PACKET_LAYER_TYPE_DATA, .data = packet_offset(l0->data, s0), .size = s1};
}

static void packet_frame_data(packet_frame *f, void *data, size_t size)
{
  packet_frame_push(f, PACKET_LAYER_TYPE_DATA, data, size);
}

static void packet_frame_udp_data(packet_frame *f, void *data, size_t size)
{
  packet_frame_data(f, data, size);
}

static void packet_frame_udp(packet_frame *f, void *data, size_t size)
{
  size_t n = sizeof (struct udphdr);

  if (size < n)
    return;
  packet_frame_push(f, PACKET_LAYER_TYPE_UDP, data, n);
  if (size == n)
    return;
  packet_frame_udp_data(f, packet_offset(data, n), size - n);
}

static void packet_frame_ip_data(packet_frame *f, void *data, size_t size)
{
  switch (packet_frame_top(f)->ip->protocol)
    {
    case IPPROTO_UDP:
      packet_frame_udp(f, data, size);
      break;
    case IPPROTO_TCP:
      packet_frame_data(f, data, size);
      packet_frame_tcp(f);
      break;
    case IPPROTO_ICMP:
      packet_frame_data(f, data, size);
      packet_frame_icmp(f);
      break;
    }
}

static void packet_frame_ip(packet_frame *f, void *data, size_t size)
{
  struct iphdr *ip = data;
  size_t n;

  if (size < sizeof *ip)
    return;
  n = ip->ihl << 2;
  if (size < n)
    return;
  packet_frame_push(f, PACKET_LAYER_TYPE_IP, ip, n);
  if (size == n)
    return;
  packet_frame_ip_data(f, packet_offset(data, n), size - n);
}

static void packet_frame_ether_data(packet_frame *f, void *data, size_t size)
{
  switch (ntohs(packet_frame_top(f)->ether->h_proto))
    {
    case ETH_P_IP:
      packet_frame_ip(f, data, size);
      break;
    default:
      packet_frame_data(f, data, size);
      break;
    }
}

static void packet_frame_ether(packet_frame *f, void *data, size_t size)
{
  size_t n = sizeof (struct ethhdr);

  if (size < n)
    return;
  packet_frame_push(f, PACKET_LAYER_TYPE_ETHER, data, n);
  if (size == n)
    return;
  packet_frame_ether_data(f, packet_offset(data, n), size - n);
}

void packet_frame_link_data(packet_frame *f, void *data, size_t size)
{
  switch (packet_frame_top(f)->sll->sll_hatype)
    {
    case ARPHRD_LOOPBACK:
    case ARPHRD_ETHER:
      packet_frame_ether(f, data, size);
      break;
    default:
      packet_frame_data(f, data, size);
      break;
    }
}

void packet_frame_link(packet_frame *f, void *data, size_t size)
{
  size_t n = sizeof (struct sockaddr_ll);

  if (size < n)
    return;
  packet_frame_push(f, PACKET_LAYER_TYPE_LINK, data, n);
  if (size == n)
    return;
  packet_frame_link_data(f, packet_offset(data, n), size - n);
}

void packet_frame_pop(packet_frame *f, int type, void *data, size_t *size)
{
  packet_layer *l = packet_frame_top(f);
  int valid = f->layers && (type == 0 || type == l->type);

  *(void **)data = valid ? l->data : NULL;
  *size = valid ? l->size : 0;
  f->layers -= valid;
}

void packet_frame_push(packet_frame *f, int type, void *data, size_t size)
{
  if (!data || f->layers >= PACKET_LAYERS_MAX)
    return;
  f->layers ++;
  *packet_frame_top(f) = (packet_layer) {.type = type, .data = data, .size = size};
}

/* public */

void packet_frame_construct(packet_frame *f)
{
  *f = (packet_frame) {0};
}

int packet_frame_size(packet_frame *f)
{
  int i, size;

  size = 0;
  for (i = 0; i < f->layers; i ++)
    size += f->layer[i].size;
  return size;
}

void packet_frame_copy(packet_frame *to, packet_frame *from)
{
  void *d;
  int i;

  to->memory = malloc(packet_frame_size(from));
  if (!to->memory)
    abort();
  d = to->memory;
  to->layers = from->layers;
  for (i = 0; i < to->layers; i ++)
    {
      to->layer[i].type = from->layer[i].type;
      to->layer[i].size = from->layer[i].size;
      to->layer[i].data = d;
      memcpy(to->layer[i].data, from->layer[i].data, to->layer[i].size);
      d = packet_offset(d, to->layer[i].size);
    }
}

void packet_frame_release(packet_frame *f)
{
  free(f->memory);
}
