# Project Requirements Summary

## Project 1 Task 1: Sender and Receiver Responsibilities

### **Sender Responsibilities**
1. **Sending Packets with a Fixed Window Size**  
   - Maintain a sending buffer of 10 packets. Queue packets within the buffer and ensure the window size is not exceeded.

2. **Managing Sequence Numbers**  
   - Assign a unique sequence number to each outgoing packet.

3. **Retransmission Timer**  
   - Start a timer when sending a packet. Retransmit the oldest unacknowledged packet if no acknowledgment is received before the timer expires.

4. **Sliding Window Management**  
   - Use `Base` and `NextSeqNum` pointers to manage the sending window. Advance the window based on received acknowledgments without exceeding the window size.

5. **Buffering or Regenerating Packets**  
   - Retain packets for retransmission between `LastPacketAcked` and `LastPacketAvailable` in memory or regenerate from the data source.

6. **Handling Duplicate ACKs**  
   - Retransmit the packet with the smallest sequence number if three consecutive duplicate ACKs are received.

7. **Timer Handling**  
   - Start a timer after sending a packet, waiting for an acknowledgment.

8. **Termination**  
   - Terminate after successfully sending all packets and receiving acknowledgment for the last one.

---

### **Receiver Responsibilities**
1. **Receiving Packets and Sending Cumulative ACKs**  
   - Use a receiving buffer to collect packets. Send cumulative ACKs for all received packets.

2. **Out-of-Order Packet Handling**  
   - Buffer out-of-order packets and send duplicate ACKs for the expected sequence number.

3. **ACK Handling**  
   - Update `NextPacketExpected` and send cumulative ACKs based on received packets.

4. **Packet Loss Detection**  
   - Detect loss via duplicate ACKs or timeouts. Inform the sender of missing packets.

5. **Termination**  
   - Acknowledge the sender's last packet to complete termination.

---

### **Shared Responsibilities**
1. **Retransmission Timer Management**  
   - Synchronize the retransmission timer between sender and receiver.

2. **Sliding Window Management**  
   - Collaborate to ensure the window adjusts dynamically based on ACKs.

3. **Packet Loss Detection**  
   - Utilize sender timeouts and receiver duplicate ACKs to detect and address packet loss.

---

## Project 1 Task 2: Additional Features

### **Retransmission Timer**
1. **Round Trip Time (RTT) Estimation**  
   - Measure RTT using timestamps for sent packets and acknowledgments. Use exponential moving averages to estimate RTT.  
   - Calculate Retransmission Timeout (RTO) as twice the estimated RTT.

2. **Karn's Algorithm**  
   - Exclude RTT measurements from retransmitted packets.

3. **Exponential Backoff**  
   - Double the RTO for successive timeouts up to a maximum of 240 seconds.

---

### **Congestion Control**
1. **Dynamic Congestion Window (CWND) Adjustment**  
   - Use Slow Start, Congestion Avoidance, and Fast Retransmit.  
   - Start CWND at 1 packet and increase exponentially during Slow Start.  
   - Switch to Congestion Avoidance when a threshold (`ssthresh`) is reached.  

2. **Packet Loss Response**  
   - Reduce CWND to `ssthresh` and adjust dynamically to avoid congestion.

---

## **CSV File (cwnd.csv) Analysis**
1. **Columns**  
   - **Time Stamp**: Marks updates to CWND.  
   - **CWND Value**: Current congestion window size.  
   - **Slow Start Threshold (ssthresh)**: Indicates the threshold for transitioning to Congestion Avoidance.

2. **Patterns to Identify**  
   - Consistent CWND growth in Slow Start.  
   - Transition to Congestion Avoidance.  
   - Steady CWND increases in stable states.  
   - Abrupt drops indicating packet loss.  
   - Logical progression of timestamps.

---

### Slow Start Phase
- Begins with CWND = 1.  
- Increases exponentially until reaching `ssthresh`.  
- Probes network bandwidth during this phase.

---
