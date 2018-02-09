#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>

#include <libnetlink.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include "packet_header.h"
#include "packet_route.h"

struct packet_route_request
{
  struct nlmsghdr nlmsg;
  struct rtmsg    rtm;
  uint8_t         data[1024];
};

static int packet_route_resolve_ip_dst(packet_route *r, char *node)
{
  struct addrinfo *ai;
  struct sockaddr_in *sin;
  int e;

  e = getaddrinfo(node, NULL, (struct addrinfo []){{.ai_family = AF_INET}}, &ai);
  if (e == -1 || !ai)
    return -1;

  sin = (struct sockaddr_in *) ai->ai_addr;
  if (sin->sin_family == AF_INET)
    r->ip_dst = sin->sin_addr.s_addr;
  freeaddrinfo(ai);

  return r->ip_dst ? 0 : -1;
}

static int packet_route_resolve_gateway(packet_route *r)
{
  int s;
  struct packet_route_request req;
  struct rtattr *rta;
  ssize_t n;

  req = (struct packet_route_request) {
    .nlmsg.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)),
    .nlmsg.nlmsg_flags = NLM_F_REQUEST,
    .nlmsg.nlmsg_type = RTM_GETROUTE,
    .rtm.rtm_family = AF_INET
  };

  rta = RTM_RTA(&req.rtm);
  rta->rta_type = RTA_DST;
  rta->rta_len = RTA_LENGTH(sizeof r->ip_dst);
  memcpy(RTA_DATA(rta), &r->ip_dst, sizeof r->ip_dst);
  req.nlmsg.nlmsg_len += rta->rta_len;

  s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (s == -1)
    return -1;
  n = send(s, &req, req.nlmsg.nlmsg_len, 0);
  if (n != -1)
    n = recv(s, &req, sizeof(req), 0);
  (void) close(s);
  if (n == -1 || n < (ssize_t) NLMSG_LENGTH(sizeof(struct rtmsg)) || req.nlmsg.nlmsg_type == NLMSG_ERROR ||
      n != req.nlmsg.nlmsg_len)
    return -1;

  for (rta = RTM_RTA(&req.rtm), n = NLMSG_PAYLOAD(&req.nlmsg, 0); RTA_OK(rta, n); rta = RTA_NEXT(rta, n))
    switch (rta->rta_type)
      {
      case RTA_OIF:
        if (rta->rta_len != RTA_LENGTH(sizeof r->index))
          break;
        memcpy(&r->index, RTA_DATA(rta), sizeof r->index);
        break;
      case RTA_GATEWAY:
        if (rta->rta_len != RTA_LENGTH(sizeof r->gateway))
          break;
        memcpy(&r->gateway, RTA_DATA(rta), sizeof r->gateway);
        break;
      }

  if (!r->gateway)
    r->gateway = r->ip_dst;

  return r->index && r->gateway ? 0 : -1;
}

static int packet_route_resolve_ether_src(packet_route *r)
{
  struct ifreq ifr = {0};
  int s, e;

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1)
    return -1;

  (void) if_indextoname(r->index, ifr.ifr_name);
  e = ioctl(s, SIOCGIFHWADDR, &ifr);
  (void) close(s);
  if (e == -1)
    return -1;

  r->link_type = ifr.ifr_hwaddr.sa_family;
  if (r->link_type != ARPHRD_ETHER && r->link_type != ARPHRD_LOOPBACK)
    return -1;
  memcpy(r->ether_src, ifr.ifr_hwaddr.sa_data, sizeof r->ether_src);

  return 0;
}

static int packet_route_resolve_ip_src(packet_route *r)
{
  struct ifreq ifr = {.ifr_addr.sa_family = AF_INET};
  int s, e;

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1)
    return -1;

  (void) if_indextoname(r->index, ifr.ifr_name);
  e = ioctl(s, SIOCGIFADDR, &ifr);
  (void) close(s);
  if (e == -1)
    return -1;

  r->ip_src = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr;

  return 0;
}

static int packet_route_resolve_ether_dst(packet_route *r)
{
  struct arpreq arp = {0};
  int s, e;

  if (r->link_type == ARPHRD_LOOPBACK)
    return 0;

  arp.arp_pa.sa_family = AF_INET;
  ((struct sockaddr_in *) &arp.arp_pa)->sin_addr.s_addr = r->gateway;
  arp.arp_ha.sa_family = r->link_type;
  (void) if_indextoname(r->index, arp.arp_dev);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1)
    return -1;
  e = ioctl(s, SIOCGARP, &arp);
  (void) close(s);
  if (e == -1 || (arp.arp_flags & ATF_COM) == 0)
    return -1;

  memcpy(r->ether_dst, arp.arp_ha.sa_data, sizeof r->ether_dst);
  return 0;
}

int packet_route_construct(packet_route *r, char *node)
{
  int e;

  *r = (packet_route) {0};
  e = packet_route_resolve_ip_dst(r, node);
  if (e == 0)
    e = packet_route_resolve_gateway(r);
  if (e == 0)
    e = packet_route_resolve_ether_src(r);
  if (e == 0)
    e = packet_route_resolve_ip_src(r);
  if (e == 0)
    e = packet_route_resolve_ether_dst(r);
  return e;
}

void packet_route_debug(packet_route *r, FILE *out)
{
  char name[IFNAMSIZ];

  (void) if_indextoname(r->index, name);
  (void) fprintf(out, "[route %s ", name);
  (void) fprintf(out, "%s/", ether_ntoa((struct ether_addr *) r->ether_src));
  (void) fprintf(out, "%s -> ", inet_ntoa(*(struct in_addr *) &r->ip_src));
  (void) fprintf(out, "%s/", ether_ntoa((struct ether_addr *) r->ether_dst));
  (void) fprintf(out, "%s", inet_ntoa(*(struct in_addr *) &r->ip_dst));
  (void) fprintf(out, "(%s)]\n", inet_ntoa(*(struct in_addr *) &r->gateway));
}

void packet_route_ether(packet_route *r, struct ethhdr *h, uint16_t proto)
{
  packet_header_ether(h, r->ether_src, r->ether_dst, proto);
}

void packet_route_ip(packet_route *r, struct iphdr *ip, size_t size, int protocol)
{
  packet_header_ip(ip, size, protocol, r->ip_src, r->ip_dst);
}
