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

diff testfile.bin received_file.bin
md5sum testfile.bin received_file.bin


./rdt_receiver 8080 received_file.bin
./rdt_sender 127.0.0.1 8080 testfile.bin


./rdt_sender $MAHIMAHI_BASE 8080 testfile.bin



dd if=/dev/zero of=testfile.bin bs=1024 count=1024

mm-delay 5 mm-loss uplink 0.2 mm-loss downlink 0.5