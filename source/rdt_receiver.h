#ifndef RDT_RECEIVER_H_INCLUDED
#define RDT_RECEIVER_H_INCLUDED

#define WINDOW_SIZE 65536
#define SEQ_NUM_SPACE UINT32_MAX

#include "packet.h"
#include "common.h"

typedef struct {
    tcp_packet *pkt;
    int received;  // 0 if not received, 1 if received
} WindowEntry;

void convert_header_from_network_order(tcp_packet *pkt);
void convert_header_to_network_order(tcp_packet *pkt);
void send_ack_packet(uint32_t ackno, int ctr_flags, struct sockaddr_in *clientaddr, socklen_t clientlen);
void slide_window();

#endif