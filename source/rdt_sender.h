#ifndef RDT_SENDER_H_INCLUDED
#define RDT_SENDER_H_INCLUDED

#define WINDOW_SIZE 65536
#define SEQ_NUM_SPACE UINT32_MAX

#include "packet.h"
#include "common.h"

// RTT estimation constants
#define ALPHA 0.125        // 1/8
#define BETA 0.25          // 1/4
#define K 4                // Multiplier for RTTVAR in RTO calculation
// #define RTO_INITIAL 3000   // Initial RTO value in milliseconds (3 seconds)
// #define RTO_MIN 1000       // Minimum RTO (1 second)
// #define RTO_MAX 240000     // Maximum RTO (240 seconds)

#define RTO_INITIAL 100.0  // Start with 100ms
#define RTO_MIN 50.0      // Minimum RTO of 50ms
#define RTO_MAX 240000    // Maximum RTO of 1 second

// Sequence number comparison macros
#define SEQ_LT(a,b) ((int32_t)((a)-(b)) < 0)
#define SEQ_LEQ(a,b) ((int32_t)((a)-(b)) <= 0)

typedef struct {
    tcp_packet *pkt;
    int acked;
    struct timeval sent_time;
    int measured_RTT;     // 1 if RTT measurement is valid, 0 otherwise (Karn's Algorithm)
} WindowEntry;

void convert_header_from_network_order(tcp_packet *pkt);
void convert_header_to_network_order(tcp_packet *pkt);
void send_data_packet(tcp_packet *pkt, int data_size);
void check_timeouts();
void update_rto_on_ack(WindowEntry *entry);
double get_time_since_start();

#endif