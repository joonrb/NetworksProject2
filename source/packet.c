#include <stdlib.h>
#include "packet.h"

static tcp_packet zero_packet = {.hdr={0}};

tcp_packet* make_packet(int len) {
    tcp_packet *pkt;
    pkt = malloc(sizeof(tcp_packet) + len);
    if (pkt == NULL) {
        perror("malloc");
        exit(1);
    }
    *pkt = zero_packet;
    pkt->hdr.data_size = len;
    return pkt;
}

int get_data_size(tcp_packet *pkt) {
    return pkt->hdr.data_size;
}
