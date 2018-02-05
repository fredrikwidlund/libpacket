#ifndef PACKET_H_INCLUDED
#define PACKET_H_INCLUDED

enum packet_type
{
  PACKET_TYPE_READER,
  PACKET_TYPE_WRITER
};

enum packet_event
{
  PACKET_EVENT_ERROR,
  PACKET_EVENT_READ,
  PACKET_EVENT_WRITE
};

enum packet_flag
{
  PACKET_FLAG_WAIT    = (1 << 0),
  PACKET_FLAG_BLOCKED = (1 << 1)
};

typedef struct packet packet;

struct packet
{
  reactor_user          user;
  int                   type;
  int                   flags;
  list                  queue;

  reactor_descriptor    descriptor;
  size_t                frame_size;
  size_t                block_size;
  size_t                block_count;
  size_t                block_current;
  size_t                frame_current;
  void                 *map;
  size_t                ring_size;
};

int  packet_open(packet *, reactor_user_callback *, void *, int, char *, size_t, size_t, size_t);
void packet_close(packet *);
void packet_write(packet *, packet_frame *);
int  packet_flush(packet *);
int  packet_blocked(packet *);

#endif /* PACKET_H_INCLUDED */
