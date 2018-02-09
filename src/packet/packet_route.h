#ifndef PACKET_ROUTE_H_INCLUDED
#define PACKET_ROUTE_H_INCLUDED

typedef struct packet_route packet_route;
struct packet_route
{
  struct addrinfo *ai;
  int              index;
  int              link_type;
  uint8_t          ether_src[ETH_ALEN];
  uint8_t          ether_dst[ETH_ALEN];
  uint32_t         ip_src;
  uint32_t         ip_dst;
  uint32_t         gateway;
};

int  packet_route_construct(packet_route *, char *);
void packet_route_debug(packet_route *, FILE *);
void packet_route_ether(packet_route *, struct ethhdr *, uint16_t);
void packet_route_ip(packet_route *, struct iphdr *, size_t, int);

#endif /* PACKET_ROUTE_H_INCLUDED */
