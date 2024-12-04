# 50ms delay, 12Mbps link
mm-delay 50 \
mm-link --meter-uplink --meter-downlink \
/usr/share/mahimahi/traces/bw12.mahi /usr/share/mahimahi/traces/bw12.mahi \
-- sh -c "./rdt_receiver 8888 receiver_file.txt & sleep 1; ./rdt_sender receiver_host 8888 sender_file.txt"

# 50ms delay, 12Mbps link, 10% loss
mm-delay 50 \
mm-loss uplink 0.1 \
mm-loss downlink 0.1 \
mm-link --meter-uplink --meter-downlink \
/usr/share/mahimahi/traces/bw12.mahi /usr/share/mahimahi/traces/bw12.mahi \
-- sh -c "./rdt_receiver 8888 receiver_file.txt & sleep 1; ./rdt_sender receiver_host 8888 sender_file.txt"


# 50ms delay, variable link
mm-delay 50 \
mm-link --meter-uplink --meter-downlink \
/usr/share/mahimahi/traces/variable.mahi /usr/share/mahimahi/traces/variable.mahi \
-- sh -c "./rdt_receiver 8888 receiver_file.txt & sleep 1; ./rdt_sender receiver_host 8888 sender_file.txt"


mm-delay 50 -- sh -c "./rdt_receiver 8888 input.txt & sleep 1; ./rdt_sender localhost 8888 output.txt"