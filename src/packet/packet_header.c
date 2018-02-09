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

#include "packet_header.h"

uint32_t packet_header_checksum_add(uint32_t sum, void *data, size_t size)
{
  size_t i;

  for (i = 0; i < size / 2; i ++)
    sum += ((uint16_t *) data)[i];

  if (size % 2)
    sum += ((uint8_t *) data)[size - 1];

  return sum;
}

uint16_t packet_header_checksum_final(uint32_t sum)
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
  ip->check = packet_header_checksum_final(packet_header_checksum_add(0, ip, sizeof(struct iphdr)));
}

void packet_header_icmp(struct icmphdr *icmp, int type, int code, void *data, size_t size)
{
  uint32_t sum;

  *icmp = (struct icmphdr) {
    .type = type,
    .code = code
  };
  sum = packet_header_checksum_add(0, icmp, sizeof *icmp);
  sum = packet_header_checksum_add(sum, data, size);
  icmp->checksum = packet_header_checksum_final(sum);
}
