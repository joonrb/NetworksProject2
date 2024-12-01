#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>

#include "rdt_sender.h"

FILE *cwnd_log;
struct timeval start_time;

int next_seqno = 0;
int send_base = 0;
int sockfd, serverlen;
struct sockaddr_in serveraddr;
FILE *fp;
int done_reading = 0;
int eot_sent = 0;
int eot_ack_received = 0;

WindowEntry window[SEQ_NUM_SPACE];  // Window entries with larger sequence number space

// RTT estimation variables
double SRTT = 0.0;    // Smoothed RTT in milliseconds
double RTTVAR = 0.0;  // RTT Variance in milliseconds
double RTO = 3000.0;  // Retransmission Timeout in milliseconds (initially 3 seconds)

// Congestion control variables
double CWND = 1.0;            // Congestion Window Size (in packets)
double ssthresh = 64.0;       // Slow Start Threshold
int last_ackno = -1;          // Last ACK number received
int dup_ack_count = 0;        // Duplicate ACK counter

int main(int argc, char **argv) {
    int portno;
    char *hostname;
    char buffer[DATA_SIZE];
    struct timeval tv;

    if (argc != 4) {
        fprintf(stderr,"usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    fp = fopen(argv[3], "rb");
    if (fp == NULL) {
        error(argv[3]);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    // Bind the sender's socket to a local address
    struct sockaddr_in senderaddr;
    bzero((char *)&senderaddr, sizeof(senderaddr));
    senderaddr.sin_family = AF_INET;
    senderaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    senderaddr.sin_port = htons(0); // Let OS choose the port

    if (bind(sockfd, (struct sockaddr *)&senderaddr, sizeof(senderaddr)) < 0) {
        error("ERROR on binding sender socket");
    }

    // Set socket timeout (optional, can be adjusted)
    tv.tv_sec = 0;
    tv.tv_usec = 0;  // We'll use select() with dynamic timeouts
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
    }

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
        fprintf(stderr,"ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    memset(window, 0, sizeof(window));

    cwnd_log = fopen("CWND.csv", "w");
    if (cwnd_log == NULL) {
        error("ERROR opening CWND.csv");
    }
    fprintf(cwnd_log, "time,CWND\n");
    gettimeofday(&start_time, NULL);

    while (1) {
        // Send packets while window is not full and data remains to be sent
        while (((next_seqno - send_base + SEQ_NUM_SPACE) % SEQ_NUM_SPACE) < floor(CWND) && !done_reading) {
            int len = fread(buffer, 1, DATA_SIZE, fp);
            if (len <= 0) {
                done_reading = 1;
                break;
            }

            tcp_packet *pkt = make_packet(len);
            memcpy(pkt->data, buffer, len);
            pkt->hdr.seqno = next_seqno;
            pkt->hdr.data_size = len;

            int idx = next_seqno % SEQ_NUM_SPACE;

            // Store the packet in the window buffer
            window[idx].pkt = pkt;
            window[idx].acked = 0;
            window[idx].retransmissions = 0;
            window[idx].measured_RTT = 1;  // RTT measurement is valid initially
            gettimeofday(&window[idx].sent_time, NULL);

            send_data_packet(pkt, len);

            fprintf(stderr, "Sent packet with seqno %d, data_size %d\n",
                    pkt->hdr.seqno, pkt->hdr.data_size);

            next_seqno = (next_seqno + 1) % SEQ_NUM_SPACE;
        }

        // Calculate the timeout for select()
        struct timeval timeout;
        struct timeval *timeout_ptr = NULL;
        long min_timeout = RTO;  // Start with RTO as the maximum possible timeout

        int has_unacked_packets = 0;
        for (int i = send_base; i != next_seqno; i = (i + 1) % SEQ_NUM_SPACE) {
            int idx = i % SEQ_NUM_SPACE;
            if (window[idx].pkt != NULL && !window[idx].acked) {
                has_unacked_packets = 1;
                struct timeval now;
                gettimeofday(&now, NULL);
                long elapsed = (now.tv_sec - window[idx].sent_time.tv_sec) * 1000 +
                               (now.tv_usec - window[idx].sent_time.tv_usec) / 1000;
                long time_remaining = RTO - elapsed;
                if (time_remaining < min_timeout) {
                    min_timeout = time_remaining;
                }
            }
        }

        // If there are no unacknowledged packets, wait indefinitely
        if (!has_unacked_packets && !done_reading) {
            timeout_ptr = NULL; // Wait indefinitely
        } else {
            // Ensure the timeout is not negative
            if (min_timeout < 0) {
                min_timeout = 0;
            }
            timeout.tv_sec = min_timeout / 1000;
            timeout.tv_usec = (min_timeout % 1000) * 1000;
            timeout_ptr = &timeout;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        int n = select(sockfd + 1, &readfds, NULL, NULL, timeout_ptr);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("select");
            }
        } else if (n == 0) {
            // Timeout occurred
            check_timeouts();
        } else {
            if (FD_ISSET(sockfd, &readfds)) {
                // Receive ACKs
                char ackbuf[1024];
                struct sockaddr_in from;
                socklen_t fromlen = sizeof(from);
                int ack_len = recvfrom(sockfd, ackbuf, sizeof(ackbuf), 0,
                                       (struct sockaddr *)&from, &fromlen);
                if (ack_len < 0) {
                    if (errno == EINTR) {
                        continue;
                    } else {
                        perror("recvfrom");
                    }
                } else {
                    // Process ACK
                    if (ack_len < TCP_HDR_SIZE) {
                        fprintf(stderr, "Received ACK packet too small\n");
                        continue;
                    }
                    tcp_packet *ack_pkt = (tcp_packet *)ackbuf;

                    convert_header_from_network_order(ack_pkt);

                    char ack_from_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(from.sin_addr), ack_from_ip, INET_ADDRSTRLEN);
                    fprintf(stderr, "Received ACK from %s:%d\n", ack_from_ip, ntohs(from.sin_port));

                    if (ack_pkt->hdr.ctr_flags == EOT_PACKET) {
                        fprintf(stderr, "Received EOT ACK\n");
                        eot_ack_received = 1;
                        break;
                    } else {
                        int ackno = ack_pkt->hdr.ackno;
                        fprintf(stderr, "Received ACK for seqno %d\n", ackno - 1);

                        if (ackno == last_ackno) {
                            dup_ack_count++;
                            if (dup_ack_count == 3) {
                                // Fast retransmit
                                fprintf(stderr, "Fast retransmit triggered for seqno %d\n", ackno);
                                ssthresh = fmax(CWND / 2, 2);
                                CWND = 1.0;
                                fprintf(stderr, "Fast retransmit: ssthresh set to %.2f, CWND set to %.2f\n", ssthresh, CWND);

                                // Log to CWND.csv
                                double elapsed_time = get_time_since_start();
                                fprintf(cwnd_log, "%.6f,%.2f\n", elapsed_time, CWND);
                                fflush(cwnd_log);


                                // Exponential backoff of RTO
                                RTO *= 2;
                                if (RTO > 240000) {
                                    RTO = 240000;
                                }

                                // Retransmit the packet
                                int idx = ackno % SEQ_NUM_SPACE;
                                if (window[idx].pkt != NULL) {
                                    send_data_packet(window[idx].pkt, window[idx].pkt->hdr.data_size);
                                    gettimeofday(&window[idx].sent_time, NULL);
                                    window[idx].measured_RTT = 0;
                                    fprintf(stderr, "Retransmitted packet with seqno %d due to fast retransmit; RTO is now %.2f ms\n",
                                            window[idx].pkt->hdr.seqno, RTO);
                                }
                                dup_ack_count = 0; // Reset duplicate ACK count
                            }
                        } else {
                            // New ACK received
                            dup_ack_count = 0;
                            last_ackno = ackno;

                            if (((ackno - send_base + SEQ_NUM_SPACE) % SEQ_NUM_SPACE) <= SEQ_NUM_SPACE) {
                                // Acknowledge all packets up to ackno - 1
                                while (send_base != ackno) {
                                    int idx = send_base % SEQ_NUM_SPACE;
                                    if (window[idx].pkt != NULL) {
                                        window[idx].acked = 1;

                                        // Update RTT estimations if applicable
                                        update_rto_on_ack(&window[idx]);

                                        // Free the packet
                                        free(window[idx].pkt);
                                        window[idx].pkt = NULL;
                                    }
                                    send_base = (send_base + 1) % SEQ_NUM_SPACE;
                                }

                                // Update CWND
                                if (CWND < ssthresh) {
                                    // Slow start
                                    CWND += 1.0;
                                    fprintf(stderr, "Slow start: CWND increased to %.2f\n", CWND);

                                    // Log to CWND.csv
                                    double elapsed_time = get_time_since_start();
                                    fprintf(cwnd_log, "%.6f,%.2f\n", elapsed_time, CWND);
                                    fflush(cwnd_log);
                                } else {
                                    // Congestion avoidance
                                    CWND += 1.0 / CWND;
                                    fprintf(stderr, "Congestion avoidance: CWND increased to %.2f\n", CWND);

                                    // Log to CWND.csv
                                    double elapsed_time = get_time_since_start();
                                    fprintf(cwnd_log, "%.6f,%.2f\n", elapsed_time, CWND);
                                    fflush(cwnd_log);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Check for timeouts
        check_timeouts();

        // Check if all data has been sent and acknowledged
        if (done_reading && send_base == next_seqno && !eot_sent) {
            // Send EOT packet
            tcp_packet *eot_pkt = make_packet(0);
            eot_pkt->hdr.ctr_flags = EOT_PACKET;
            eot_pkt->hdr.seqno = next_seqno;

            send_data_packet(eot_pkt, 0);

            fprintf(stderr, "Sent EOT packet with seqno %d\n", next_seqno);

            eot_sent = 1;
        }

        if (eot_sent && eot_ack_received) {
            fprintf(stderr, "All data sent and acknowledged. Exiting.\n");
            break;
        }
    }

    fclose(fp);

    // Clean up remaining packets
    for (int i = 0; i < SEQ_NUM_SPACE; i++) {
        if (window[i].pkt != NULL) {
            free(window[i].pkt);
        }
    }

    fclose(cwnd_log);

    return 0;
}

/* Function to convert packet header from network byte order to host byte order */
void convert_header_from_network_order(tcp_packet *pkt) {
    pkt->hdr.seqno = ntohl(pkt->hdr.seqno);
    pkt->hdr.ackno = ntohl(pkt->hdr.ackno);
    pkt->hdr.ctr_flags = ntohl(pkt->hdr.ctr_flags);
    pkt->hdr.data_size = ntohl(pkt->hdr.data_size);
}

/* Function to convert packet header from host byte order to network byte order */
void convert_header_to_network_order(tcp_packet *pkt) {
    pkt->hdr.seqno = htonl(pkt->hdr.seqno);
    pkt->hdr.ackno = htonl(pkt->hdr.ackno);
    pkt->hdr.ctr_flags = htonl(pkt->hdr.ctr_flags);
    pkt->hdr.data_size = htonl(pkt->hdr.data_size);
}

/* Function to send a data packet */
void send_data_packet(tcp_packet *pkt, int data_size) {
    char server_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(serveraddr.sin_addr), server_ip, INET_ADDRSTRLEN);
    fprintf(stderr, "Sending packet to %s:%d\n", server_ip, ntohs(serveraddr.sin_port));

    convert_header_to_network_order(pkt);
    if (sendto(sockfd, pkt, TCP_HDR_SIZE + data_size, 0,
               (const struct sockaddr *)&serveraddr, serverlen) < 0) {
        error("sendto");
    }
    convert_header_from_network_order(pkt);
}

/* Function to check for timeouts and retransmit packets */
void check_timeouts() {
    struct timeval now;
    gettimeofday(&now, NULL);

    for (int i = send_base; i != next_seqno; i = (i + 1) % SEQ_NUM_SPACE) {
        int idx = i % SEQ_NUM_SPACE;
        if (window[idx].pkt != NULL && !window[idx].acked) {
            long elapsed = (now.tv_sec - window[idx].sent_time.tv_sec) * 1000 +
                           (now.tv_usec - window[idx].sent_time.tv_usec) / 1000;
            if (elapsed >= RTO) {
                // Timeout occurred
                ssthresh = fmax(CWND / 2, 2);
                CWND = 1.0;
                fprintf(stderr, "Timeout occurred for seqno %d. ssthresh set to %.2f, CWND set to %.2f\n",
                        window[idx].pkt->hdr.seqno, ssthresh, CWND);

                // Log to CWND.csv
                double elapsed_time = get_time_since_start();
                fprintf(cwnd_log, "%.6f,%.2f\n", elapsed_time, CWND);
                fflush(cwnd_log);

                // Exponential backoff of RTO
                RTO *= 2;
                if (RTO > 240000) {
                    RTO = 240000;
                }

                // Karn's Algorithm: Do not update RTT estimations for retransmitted packets
                window[idx].measured_RTT = 0;

                // Retransmit the packet
                send_data_packet(window[idx].pkt, window[idx].pkt->hdr.data_size);

                // Update sent_time
                gettimeofday(&window[idx].sent_time, NULL);

                fprintf(stderr, "Retransmitted packet with seqno %d due to timeout; RTO is now %.2f ms\n",
                        window[idx].pkt->hdr.seqno, RTO);

                // Break after retransmitting due to timeout
                break;
            }
        }
    }
}

/* Function to update RTO upon receiving an ACK */
void update_rto_on_ack(WindowEntry *entry) {
    if (entry->measured_RTT) {
        struct timeval now;
        gettimeofday(&now, NULL);
        double RTT_sample = (now.tv_sec - entry->sent_time.tv_sec) * 1000.0 +
                            (now.tv_usec - entry->sent_time.tv_usec) / 1000.0;

        if (SRTT == 0) {
            // First RTT measurement
            SRTT = RTT_sample;
            RTTVAR = RTT_sample / 2;
        } else {
            RTTVAR = (1 - BETA) * RTTVAR + BETA * fabs(SRTT - RTT_sample);
            SRTT = (1 - ALPHA) * SRTT + ALPHA * RTT_sample;
        }

        // Update RTO
        RTO = SRTT + K * RTTVAR;
        if (RTO < RTO_MIN) {
            RTO = RTO_MIN;
        } else if (RTO > 240000) {
            RTO = 240000; // Upper bound for RTO
        }

        fprintf(stderr, "Updated RTO to %.2f ms based on RTT sample %.2f ms\n", RTO, RTT_sample);
    }

    // Reset retransmission count and measured_RTT flag
    entry->retransmissions = 0;
    entry->measured_RTT = 1; // Allow RTT measurement for future transmissions
}

double get_time_since_start() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = (now.tv_sec - start_time.tv_sec) + (now.tv_usec - start_time.tv_usec) / 1e6;
    return elapsed;
}