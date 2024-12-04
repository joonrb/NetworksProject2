#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define DATA_PACKET 1 // data packet
#define ACK_PACKET 2 // ack packet
#define EOT_PACKET 3  // End-of-Transmission packet

#pragma pack(push, 1) // packing, no extra padding
typedef struct {
    uint32_t seqno; // sequence number for ordering packets
    uint32_t ackno; // ack number for reliable transmission
    uint32_t ctr_flags; // control flags for packet management
    uint32_t data_size; // data size
} tcp_header;
#pragma pack(pop) // end of packing, restore previous packing

#define MSS_SIZE    1500 // maximum segment size
#define UDP_HDR_SIZE    8 // UDP header size
#define IP_HDR_SIZE    20 // IP header size
#define TCP_HDR_SIZE    sizeof(tcp_header) // TCP header size
#define DATA_SIZE   (MSS_SIZE - TCP_HDR_SIZE - UDP_HDR_SIZE - IP_HDR_SIZE) // data size

typedef struct {
    tcp_header  hdr;
    char        data[];
} tcp_packet;
// the actual size of data is len + sizeof(tcp_header), determined at runtime

tcp_packet* make_packet(int len); // Creates a new packet with specified data length
int get_data_size(tcp_packet *pkt); // Returns the actual data size of the packet

#endif // PACKET_H

