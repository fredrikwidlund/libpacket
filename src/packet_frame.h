#ifndef PACKET_FRAME_H_INCLUDED
#define PACKET_FRAME_H_INCLUDED

#define PACKET_LAYERS_MAX    15

typedef struct packet_layer packet_layer;
typedef struct packet_frame packet_frame;

enum packet_layer_type
{
  PACKET_LAYER_TYPE_ANY = 0,
  PACKET_LAYER_TYPE_DATA,
  PACKET_LAYER_TYPE_LINK,
  PACKET_LAYER_TYPE_ETHER,
  PACKET_LAYER_TYPE_IP,
  PACKET_LAYER_TYPE_ICMP,
  PACKET_LAYER_TYPE_UDP,
  PACKET_LAYER_TYPE_TCP
};

struct packet_layer
{
  int                   type;
  int                   size;
  union
  {
    void               *data;
    struct sockaddr_ll *sll;
    struct ethhdr      *ether;
    struct iphdr       *ip;
    struct udphdr      *udp;
    struct tcphdr      *tcp;
    struct icmphdr     *icmp;
  };
};

struct packet_frame
{
  packet_layer          layer[PACKET_LAYERS_MAX];
  size_t                layers;
  void                 *memory;
};

void   packet_frame_pop(packet_frame *, int, void *, size_t *);
void   packet_frame_push(packet_frame *, int, void *, size_t);
size_t packet_frame_size(packet_frame *);
void   packet_frame_copy(packet_frame *, packet_frame *);
void   packet_frame_release(packet_frame *);

void   packet_frame_link_data(packet_frame *, void *, size_t);
void   packet_frame_link(packet_frame *, void *, size_t);

#endif /* PACKET_FRAME_H_INCLUDED */
