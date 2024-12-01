#ifndef RDT_RECEIVER_H_INCLUDED
#define RDT_RECEIVER_H_INCLUDED

#define WINDOW_SIZE 10
#define SEQ_NUM_SPACE 256

#include "packet.h"
#include "common.h"

typedef struct {
    tcp_packet *pkt;
    int received;  // 0 if not received, 1 if received
} WindowEntry;

void convert_header_from_network_order(tcp_packet *pkt);
void convert_header_to_network_order(tcp_packet *pkt);
void send_ack_packet(int ackno, int ctr_flags, struct sockaddr_in *clientaddr, socklen_t clientlen);
void slide_window();
int is_seqno_in_window(int seqno);

#endif
