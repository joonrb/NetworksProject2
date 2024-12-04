#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rdt_receiver.h"

/*
Socket descriptor and addressing structures
file pointer for output file
window for storing received packets
*/

int sockfd; // socket descriptor
struct sockaddr_in serveraddr, clientaddr; // server and client addressing structures
socklen_t clientlen; // client length
FILE *fp; // file pointer for output file

WindowEntry window[WINDOW_SIZE]; // window for storing received packets
int window_base = 0; // base sequence number of the window



/*
Main Program Flow:
The receiver operates in a continuous loop that:
listens for incoming packets from the sender
Processes received packets
Sends acknowledgements
Writes data to file in order
*/
int main(int argc, char **argv) {
    int portno; // port number
    char buffer[MSS_SIZE]; // buffer for receiving packets

    if (argc != 3) { // Check if the number of arguments is correct
        fprintf(stderr, "usage: %s <port> <file_to_write>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]); // port number
    fp = fopen(argv[2], "wb"); // file pointer for output file
    if (fp == NULL) {
        error("ERROR opening file");
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0); // socket descriptor
    if (sockfd < 0) // Check if the socket is created successfully
        error("ERROR opening socket");

    int optval = 1; // option value for socket reuse
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    bzero((char *) &serveraddr, sizeof(serveraddr)); // Clear the server address structure
    serveraddr.sin_family = AF_INET; // Set the address family to IPv4
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // Set the IP address to any available interface
    serveraddr.sin_port = htons((unsigned short)portno); // Set the port number

    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) // Bind the socket to the server address
        error("ERROR on binding");

    clientlen = sizeof(clientaddr); // Set the client length
    memset(window, 0, sizeof(window)); // Clear the window

    fprintf(stderr, "Server is ready to receive packets on port %d\n", portno);


    // Packet reception and processing loop
    while (1) {
        // Receive packet from sender
        int n = recvfrom(sockfd, buffer, MSS_SIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0) { // Check if the packet is received successfully
            perror("recvfrom");
            fprintf(stderr, "Failed to receive data\n");
            continue;
        } else {
            char client_ip[INET_ADDRSTRLEN]; // IP address of the client
            inet_ntop(AF_INET, &(clientaddr.sin_addr), client_ip, INET_ADDRSTRLEN);
            fprintf(stderr, "Received %d bytes from %s:%d\n", n, client_ip, ntohs(clientaddr.sin_port));
        }

        // Allocate memory for the packet and copy data from buffer
        tcp_packet *pkt = (tcp_packet *)malloc(n); // Allocate memory for the packet
        if (pkt == NULL) { // Check if the memory allocation failed
            error("malloc");
        }
        memcpy(pkt, buffer, n);

        // Convert packet header from network byte order to host byte order
        convert_header_from_network_order(pkt);

        // Extract sequence number from packet header
        int seqno = pkt->hdr.seqno;

        // Check for EOT packet
        if (pkt->hdr.ctr_flags == EOT_PACKET) { // Check if the packet is an EOT packet
            fprintf(stderr, "Received EOT packet\n");

            // Send EOT ACK
            send_ack_packet(window_base, EOT_PACKET, &clientaddr, clientlen);
            free(pkt);
            break;  // Exit the loop and terminate the receiver
        }

        fprintf(stderr, "Received packet with seqno %d, data_size %d\n", // Print the sequence number and data size of the received packet
                seqno, pkt->hdr.data_size);

        // Check if the packet is within the current window
        /*
        if the packet is within the current window, store new packets, detect duplicates, send acknowledgements and slide the window
        if packet is ahead of the window, buffer overflow protection, send current window base as ACK
        if packet is behind the window, handle delayed packets, send current window base as ACK
        */
        if (is_seqno_in_window(seqno)) {
            int idx = (seqno - window_base + SEQ_NUM_SPACE) % SEQ_NUM_SPACE; // Calculate index in window
            if (idx < WINDOW_SIZE) { // Check if the index is within the window
                if (!window[idx].received) {  // New packet
                    window[idx].pkt = pkt;
                    window[idx].received = 1;
                    fprintf(stderr, "Stored packet with seqno %d at index %d\n", seqno, idx);
                } else { // Duplicate packet
                    fprintf(stderr, "Duplicate packet with seqno %d\n", seqno);
                    free(pkt);  // Free duplicate packet
                }

                // Always send ACK for the last contiguous byte received
                send_ack_packet(window_base, 0, &clientaddr, clientlen);

                // Attempt to slide the window
                slide_window();
            } else {
                // Packet is outside the window (too far ahead)
                // Send ACK for the last contiguous byte received
                send_ack_packet(window_base, 0, &clientaddr, clientlen);
                fprintf(stderr, "Received packet outside window with seqno %d\n", seqno);
                free(pkt);
            }
        } else {
            // Packet is outside the window (behind window_base)
            // Likely a delayed packet, send ACK for the last contiguous byte received
            send_ack_packet(window_base, 0, &clientaddr, clientlen); // Send ACK for the last contiguous byte received
            fprintf(stderr, "Received out-of-window packet with seqno %d\n", seqno);
            free(pkt); // Free the memory allocated for the packet
        }
    }

    fclose(fp); // Close the output file
    return 0;
}

/* Function to convert packet header from network byte order to host byte order */
void convert_header_from_network_order(tcp_packet *pkt) {
    pkt->hdr.seqno = ntohl(pkt->hdr.seqno); // Convert sequence number from network byte order to host byte order
    pkt->hdr.ackno = ntohl(pkt->hdr.ackno); // Convert acknowledgment number from network byte order to host byte order
    pkt->hdr.ctr_flags = ntohl(pkt->hdr.ctr_flags); // Convert control flags from network byte order to host byte order
    pkt->hdr.data_size = ntohl(pkt->hdr.data_size); // Convert data size from network byte order to host byte order
}

/* Function to convert packet header from host byte order to network byte order */
void convert_header_to_network_order(tcp_packet *pkt) {
    pkt->hdr.seqno = htonl(pkt->hdr.seqno); // Convert sequence number from host byte order to network byte order
    pkt->hdr.ackno = htonl(pkt->hdr.ackno); // Convert acknowledgment number from host byte order to network byte order
    pkt->hdr.ctr_flags = htonl(pkt->hdr.ctr_flags); // Convert control flags from host byte order to network byte order
    pkt->hdr.data_size = htonl(pkt->hdr.data_size); // Convert data size from host byte order to network byte order
}

/* Function to send an ACK packet */
void send_ack_packet(int ackno, int ctr_flags, struct sockaddr_in *clientaddr, socklen_t clientlen) {
    tcp_packet *ack_pkt = make_packet(0); // Create a new packet
    ack_pkt->hdr.ackno = ackno; // Use the ackno parameter (window_base)
    ack_pkt->hdr.ctr_flags = ctr_flags; // Use the ctr_flags parameter
    convert_header_to_network_order(ack_pkt); // Convert the header to network byte order

    if (sendto(sockfd, ack_pkt, TCP_HDR_SIZE, 0, (struct sockaddr *)clientaddr, clientlen) < 0) { // Send the packet
        perror("sendto"); // Print the error message
        error("sendto");
    }

    char client_ip[INET_ADDRSTRLEN]; // IP address of the client
    inet_ntop(AF_INET, &(clientaddr->sin_addr), client_ip, INET_ADDRSTRLEN); // Convert the IP address to a string
    fprintf(stderr, "Sent ACK for seqno %d to %s:%d\n", ack_pkt->hdr.ackno - 1, client_ip, ntohs(clientaddr->sin_port));

    free(ack_pkt); // Free the memory allocated for the packet
}

/*
window sliding mechanism
Writes contiguous received data to file
Shifts window when base packet is received
updates window base
maintain packet ordering
*/
void slide_window() {
    while (window[0].received) { // Check if the first packet in the window is received
        int data_size = window[0].pkt->hdr.data_size; // Get the data size of the first packet
        fprintf(stderr, "Writing %d bytes to file from seqno %d\n", // Print the data size and sequence number of the first packet
                data_size, window[0].pkt->hdr.seqno); 
        fwrite(window[0].pkt->data, 1, data_size, fp); // Write data to file
        fflush(fp);  // Ensure data is written to disk
        free(window[0].pkt); // Free the memory allocated for the packet

        // Shift the window
        memmove(&window[0], &window[1], sizeof(WindowEntry) * (WINDOW_SIZE - 1)); // Shift window entries
        window[WINDOW_SIZE - 1].pkt = NULL; // Clear the last entry
        window[WINDOW_SIZE - 1].received = 0; 

        window_base = (window_base + 1) % SEQ_NUM_SPACE; // Update window base
    }
}

/* Function to check if a sequence number is within the current window */
int is_seqno_in_window(int seqno) {
    int rel_seqno = (seqno - window_base + SEQ_NUM_SPACE) % SEQ_NUM_SPACE; // Calculate relative sequence number
    return rel_seqno < WINDOW_SIZE; // Check if the sequence number is within the window
}
