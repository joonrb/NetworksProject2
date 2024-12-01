#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define DATA_PACKET 1
#define ACK_PACKET 2
#define EOT_PACKET 3  // End-of-Transmission packet

#pragma pack(push, 1)
typedef struct {
    uint32_t seqno;
    uint32_t ackno;
    uint32_t ctr_flags;
    uint32_t data_size;
} tcp_header;
#pragma pack(pop)

#define MSS_SIZE    1500
#define UDP_HDR_SIZE    8
#define IP_HDR_SIZE    20
#define TCP_HDR_SIZE    sizeof(tcp_header)
#define DATA_SIZE   (MSS_SIZE - TCP_HDR_SIZE - UDP_HDR_SIZE - IP_HDR_SIZE)

typedef struct {
    tcp_header  hdr;
    char        data[];
} tcp_packet;

tcp_packet* make_packet(int len);
int get_data_size(tcp_packet *pkt);

#endif // PACKET_H
