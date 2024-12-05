#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rdt_receiver.h"

int sockfd;
struct sockaddr_in serveraddr, clientaddr;
socklen_t clientlen;
FILE *fp;

WindowEntry window[WINDOW_SIZE];
uint32_t oldest_unack = 0;

int main(int argc, char **argv) {
    int portno;
    char buffer[MSS_SIZE];

    if(argc != 3) {
        fprintf(stderr, "usage: %s <port> <file_to_write>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);
    fp = fopen(argv[2], "wb");
    if(fp == NULL) {
        error("ERROR opening file");
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0)
        error("ERROR opening socket");

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    if(bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    clientlen = sizeof(clientaddr);
    memset(window, 0, sizeof(window));

    fprintf(stderr, "Server is ready to receive packets on port %d\n", portno);

    while(1) {
        int n = recvfrom(sockfd, buffer, MSS_SIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
        if(n < 0) {
            perror("recvfrom");
            fprintf(stderr, "Failed to receive data\n");
            continue;
        }else{
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientaddr.sin_addr), client_ip, INET_ADDRSTRLEN);
            fprintf(stderr, "Received %d bytes from %s:%d\n", n, client_ip, ntohs(clientaddr.sin_port));
        }

        tcp_packet *pkt = (tcp_packet *)malloc(n);
        if(pkt == NULL) {
            error("malloc");
        }
        memcpy(pkt, buffer, n);

        convert_header_from_network_order(pkt);

        uint32_t seqno = pkt->hdr.seqno;

        // Check for EOT packet
        if(pkt->hdr.ctr_flags == EOT_PACKET) {
            fprintf(stderr, "Received EOT packet\n");

            // Send EOT ACK
            send_ack_packet(oldest_unack, EOT_PACKET, &clientaddr, clientlen);
            free(pkt);
            break;  // Exit the loop and terminate the receiver
        }

        fprintf(stderr, "Received packet with seqno %u\n", seqno);

        if((seqno - oldest_unack + SEQ_NUM_SPACE) % SEQ_NUM_SPACE < WINDOW_SIZE) {
            int idx = (seqno - oldest_unack + SEQ_NUM_SPACE) % SEQ_NUM_SPACE;
            if(idx < WINDOW_SIZE) {
                if(!window[idx].received) {  // Check if the packet is new
                    window[idx].pkt = pkt;
                    window[idx].received = 1;
                    fprintf(stderr, "Stored packet with seqno %u at index %d\n", seqno, idx);
                }else{
                    fprintf(stderr, "Duplicate packet with seqno %u\n", seqno);
                    free(pkt);  // Free duplicate packet
                }

                // Always send ACK for the last contiguous byte received
                send_ack_packet(oldest_unack, 0, &clientaddr, clientlen);

                // Attempt to slide the window
                slide_window();
            }else{
                // Packet is ahead of the window
                // Send ACK for the last contiguous byte received
                send_ack_packet(oldest_unack, 0, &clientaddr, clientlen);
                fprintf(stderr, "Received packet outside window with seqno %u\n", seqno);
                free(pkt);
            }
        }else{
            // Packet is behind the window -> delayed packet
            // Send ACK for the last contiguous byte received
            send_ack_packet(oldest_unack, 0, &clientaddr, clientlen);
            fprintf(stderr, "Received out-of-window packet with seqno %u\n", seqno);
            free(pkt);
        }
    }

    fclose(fp);
    return 0;
}

// Function to convert packet header from network byte order to host byte order
void convert_header_from_network_order(tcp_packet *pkt) {
    pkt->hdr.seqno = ntohl(pkt->hdr.seqno);
    pkt->hdr.ackno = ntohl(pkt->hdr.ackno);
    pkt->hdr.ctr_flags = ntohl(pkt->hdr.ctr_flags);
    pkt->hdr.data_size = ntohl(pkt->hdr.data_size);
}

// Function to convert packet header from host byte order to network byte order
void convert_header_to_network_order(tcp_packet *pkt) {
    pkt->hdr.seqno = htonl(pkt->hdr.seqno);
    pkt->hdr.ackno = htonl(pkt->hdr.ackno);
    pkt->hdr.ctr_flags = htonl(pkt->hdr.ctr_flags);
    pkt->hdr.data_size = htonl(pkt->hdr.data_size);
}

// Function to send an ACK packet
void send_ack_packet(uint32_t ackno, int ctr_flags, struct sockaddr_in *clientaddr, socklen_t clientlen) {
    tcp_packet *ack_pkt = make_packet(0);
    ack_pkt->hdr.ackno = ackno; 
    ack_pkt->hdr.ctr_flags = ctr_flags;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientaddr->sin_addr), client_ip, INET_ADDRSTRLEN);
    fprintf(stderr, "Sent ACK for seqno %u to %s:%d\n", ack_pkt->hdr.ackno, client_ip, ntohs(clientaddr->sin_port));

    // Convert header to network byte order
    convert_header_to_network_order(ack_pkt);

    if(sendto(sockfd, ack_pkt, TCP_HDR_SIZE, 0, (struct sockaddr *)clientaddr, clientlen) < 0) {
        perror("sendto");
        error("sendto");
    }

    free(ack_pkt);
}

void slide_window() {
    while(window[0].received) {
        int data_size = window[0].pkt->hdr.data_size;
        fprintf(stderr, "Writing %d bytes to file from seqno %u\n", data_size, window[0].pkt->hdr.seqno);
        fwrite(window[0].pkt->data, 1, data_size, fp);
        fflush(fp);  // Ensure data is written to disk
        free(window[0].pkt);

        // Shift the window
        memmove(&window[0], &window[1], sizeof(WindowEntry) * (WINDOW_SIZE - 1));
        window[WINDOW_SIZE - 1].pkt = NULL;
        window[WINDOW_SIZE - 1].received = 0;

        oldest_unack = (oldest_unack + 1) % SEQ_NUM_SPACE;
    }
}