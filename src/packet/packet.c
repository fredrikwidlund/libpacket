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

#include <dynamic.h>
#include <reactor.h>

#include "packet_frame.h"
#include "packet.h"

static void *packet_offset(void *data, size_t size)
{
  return (uint8_t *) data + size;
}

static int packet_error(packet *p)
{
  return reactor_user_dispatch(&p->user, PACKET_EVENT_ERROR, NULL);
}

static int packet_configure(packet *p, int fd, char *interface)
{
  int e, index;

  index = if_nametoindex(interface);
  if (!index)
    return -1;

  e = setsockopt(fd, SOL_PACKET, PACKET_VERSION, (int []) {TPACKET_V3}, sizeof (int));
  if (e == -1)
    return -1;

  e = setsockopt(fd, SOL_PACKET, p->type == PACKET_TYPE_READER ? PACKET_RX_RING : PACKET_TX_RING, (struct tpacket_req3 []) {{
        .tp_frame_size = p->frame_size,
        .tp_frame_nr = p->block_count * (p->block_size / p->frame_size),
        .tp_block_size = p->block_size,
        .tp_block_nr = p->block_count
        }}, sizeof (struct tpacket_req3));
  if (e == -1)
    return -1;

  p->map = mmap(NULL, p->block_size * p->block_count,
                PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED | MAP_POPULATE, fd, 0);
  if (p->map == MAP_FAILED)
    return -1;
  memset(p->map, 0, p->block_size * p->block_count);

  e = bind(fd, (struct sockaddr *) (struct sockaddr_ll []){{
        .sll_family = PF_PACKET,
        .sll_protocol = htons(ETH_P_ALL),
        .sll_ifindex = index
      }}, sizeof (struct sockaddr_ll));
  if (e == -1)
    return -1;

  return 0;
}

static int packet_socket(packet *p, char *interface)
{
  int fd, e;

  fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (fd == -1)
    return -1;

  e = packet_configure(p, fd, interface);
  if (e == -1)
    {
      (void) close(fd);
      return -1;
    }

  return fd;
}

static int packet_receive(packet *p)
{
  struct tpacket_block_desc *bh;
  struct tpacket3_hdr *tp;
  packet_frame f;
  void *d;
  size_t i;
  int e;

  while (1)
    {
      bh = packet_offset(p->map, p->block_current * p->block_size);
      if (!bh->hdr.bh1.block_status & TP_STATUS_USER)
        break;
      d = packet_offset(bh, bh->hdr.bh1.offset_to_first_pkt);
      for (i = 0; i < bh->hdr.bh1.num_pkts; i ++)
        {
          tp = d;
          if (tp->tp_len == tp->tp_snaplen)
            {
              packet_frame_construct(&f);
              packet_frame_link(&f, packet_offset(d, TPACKET_ALIGN(sizeof(struct tpacket3_hdr))),
                                sizeof (struct sockaddr_ll));
              packet_frame_link_data(&f, packet_offset(d, tp->tp_mac), tp->tp_len);
              e = reactor_user_dispatch(&p->user, PACKET_EVENT_READ, &f);
              if (e == REACTOR_ABORT)
                return REACTOR_ABORT;
            }
          d = packet_offset(d, tp->tp_next_offset);
        }
      bh->hdr.bh1.block_status = TP_STATUS_KERNEL;
      p->block_current = (p->block_current + 1) % p->block_count;
    }

  return REACTOR_OK;
}

static int packet_socket_event(void *state, int type, void *data)
{
  packet *p = state;
  int e, *flags = data;

  (void) type;
  if (*flags & EPOLLIN)
    {
      e = packet_receive(p);
      if (e == REACTOR_ABORT)
        return REACTOR_ABORT;
      *flags &= ~EPOLLIN;
    }

  if (*flags & EPOLLOUT)
    {
      e = packet_flush(p);
      if (e == REACTOR_ABORT)
        return REACTOR_ABORT;
      *flags &= ~EPOLLOUT;
    }

  return *flags ? packet_error(p) : REACTOR_OK;
}

int packet_open(packet *p, reactor_user_callback *callback, void *state, int type, char *interface,
                size_t frame_size, size_t block_size, size_t block_count)
{
  int e, fd;

  *p = (packet) {.type = type, .frame_size = frame_size, .block_size = block_size, .block_count = block_count};
  reactor_user_construct(&p->user, callback, state);
  list_construct(&p->queue);

  fd = packet_socket(p, interface);
  if (fd == -1)
    return REACTOR_ERROR;

  e = reactor_descriptor_open(&p->descriptor, packet_socket_event, p, fd, EPOLLIN | EPOLLET);
  if (e == REACTOR_ERROR)
    {
      (void) close(fd);
      return REACTOR_ERROR;
    }

  return REACTOR_OK;
}

void packet_close(packet *p)
{
  reactor_descriptor_close(&p->descriptor);
  munmap(p->map, p->block_size * p->block_count);
  *p = (packet) {0};
}

static void *packet_ring_frame(packet *p)
{
  struct tpacket_block_desc *bh;
  struct tpacket3_hdr *tp;

  bh = packet_offset(p->map, p->block_current * p->block_size);
  tp = packet_offset(bh, p->frame_current * p->frame_size);
  if (tp->tp_status != TP_STATUS_AVAILABLE)
    return NULL;

  p->frame_current = (p->frame_current + 1) % (p->block_size / p->frame_size);
  if (p->frame_current == 0)
    p->block_current = (p->block_current + 1) % p->block_count;

  return tp;
}

static void packet_write_queue(packet *p, packet_frame *f)
{
  packet_frame copy;

  packet_frame_copy(&copy, f);
  list_push_back(&p->queue, &copy, sizeof copy);
  p->flags |= PACKET_FLAG_BLOCKED;
}

static void packet_write_wait(packet *p)
{
  if (p->flags & PACKET_FLAG_WAIT)
    return;
  p->flags |= PACKET_FLAG_WAIT;
  reactor_descriptor_set(&p->descriptor, EPOLLIN | EPOLLOUT | EPOLLET);
}

static void packet_write_done(packet *p)
{
  if ((p->flags & PACKET_FLAG_WAIT) == 0)
    return;
  p->flags &= ~PACKET_FLAG_WAIT;
  reactor_descriptor_set(&p->descriptor, EPOLLIN | EPOLLET);
}

static int packet_write_ring(packet *p, packet_frame *f)
{
  struct tpacket3_hdr *tp;
  void *d;
  int i, len;

  tp = packet_ring_frame(p);
  if (!tp)
    return 0;

  d = packet_offset(tp, TPACKET3_HDRLEN - sizeof(struct sockaddr_ll));
  len = 0;
  for (i = f->layer[0].type == PACKET_LAYER_TYPE_LINK ? 1 : 0 ; i < f->layers; i ++)
    {
      memcpy(d, f->layer[i].data, f->layer[i].size);
      d = packet_offset(d, f->layer[i].size);
      len += f->layer[i].size;
    }
  tp->tp_len = len;
  tp->tp_snaplen = len;
  tp->tp_status = TP_STATUS_SEND_REQUEST;
  p->ring_size += len;
  packet_write_wait(p);

  return 1;
}

void packet_write(packet *p, packet_frame *f)
{
  int n;

  n = packet_write_ring(p, f);
  if (!n)
    packet_write_queue(p, f);
}

int packet_flush(packet *p)
{
  packet_frame *f;
  ssize_t n;

  if (p->ring_size)
    {
      n = send(reactor_descriptor_fd(&p->descriptor), NULL, 0, MSG_DONTWAIT);
      if (n == -1)
        return errno == EAGAIN ? REACTOR_OK : packet_error(p);
      p->ring_size -= n;
    }

  while (!list_empty(&p->queue))
    {
      f = list_front(&p->queue);
      n = packet_write_ring(p, f);
      if (!n)
        break;
      packet_frame_release(f);
      list_erase(f, NULL);
    }

  if (!p->ring_size && list_empty(&p->queue))
    packet_write_done(p);

  if (p->flags & PACKET_FLAG_BLOCKED && list_empty(&p->queue))
    {
      p->flags &= ~PACKET_FLAG_BLOCKED;
      return reactor_user_dispatch(&p->user, PACKET_EVENT_WRITE, NULL);
    }

  return REACTOR_OK;
}

int packet_blocked(packet *p)
{
  return !list_empty(&p->queue);
}
