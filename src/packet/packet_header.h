#ifndef PACKET_HEADER_H_INCLUDED
#define PACKET_HEADER_H_INCLUDED

uint32_t packet_header_checksum_add(uint32_t, void *, size_t);
uint16_t packet_header_checksum_final(uint32_t);
void     packet_header_ether(struct ethhdr *, uint8_t *, uint8_t *, uint16_t);
void     packet_header_ip(struct iphdr *, size_t, int, uint32_t, uint32_t);
void     packet_header_icmp(struct icmphdr *, int, int, void *, size_t);
void     packet_header_udp(struct udphdr *, uint16_t, uint16_t, void *, size_t);

#endif /* PACKET_HEADER_H_INCLUDED */
