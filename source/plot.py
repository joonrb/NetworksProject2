import numpy as np
import matplotlib.pyplot as plt
import sys
from argparse import ArgumentParser

def scale(a):
    return a/1000000.0

parser = ArgumentParser(description="plot")

parser.add_argument('--dir', '-d',
                    help="Directory to store outputs",
                    required=True)

parser.add_argument('--name', '-n',
                    help="name of the experiment",
                    required=True)

parser.add_argument('--trace', '-tr',
                    help="name of the trace",
                    required=True)

args = parser.parse_args()

fig, ax1 = plt.subplots(figsize=(21,5), facecolor='w')

# plotting the trace file
f1 = open(args.trace, "r")
BW = []
nextTime = 1000
cnt = 0
for line in f1:
    if int(line.strip()) > nextTime:
        BW.append(cnt*1492*8)
        cnt = 0
        nextTime += 1000
    else:
        cnt += 1
f1.close()

ax1.fill_between(range(len(BW)), 0, list(map(scale,BW)), color='#D3D3D3', label='Bandwidth')

# plotting throughput
throughputDL = []
timeDL = []

traceDL = open(args.dir+"/"+str(args.name), 'r')
traceDL.readline()

tmp = traceDL.readline().strip().split(",")
bytes = int(tmp[1])
startTime = float(tmp[0])
stime = float(startTime)

for time in traceDL:
    if (float(time.strip().split(",")[0]) - float(startTime)) <= 1.0:
        bytes += int(time.strip().split(",")[1])
    else:
        throughputDL.append(bytes*8/1000000.0)
        timeDL.append(float(startTime)-stime)
        bytes = int(time.strip().split(",")[1])
        startTime += 1.0

traceDL.close()

ax1.plot(timeDL, throughputDL, lw=2, color='r', label='Throughput')

# plotting CWND
cwnd_time = []
cwnd_values = []

with open('CWND.csv', 'r') as cwnd_file:
    next(cwnd_file)  # Skip header
    for line in cwnd_file:
        time_str, cwnd_str = line.strip().split(',')
        cwnd_time.append(float(time_str))
        cwnd_values.append(float(cwnd_str))

# Create a second y-axis for CWND
ax2 = ax1.twinx()
ax2.plot(cwnd_time, cwnd_values, lw=2, color='b', label='CWND')

ax1.set_xlabel("Time (s)")
ax1.set_ylabel("Throughput (Mbps)", color='r')
ax2.set_ylabel("CWND (packets)", color='b')

ax1.tick_params(axis='y', labelcolor='r')
ax2.tick_params(axis='y', labelcolor='b')

ax1.grid(True, which="both")
ax1.set_xlim([0, max(timeDL + cwnd_time)])

# Combine legends
lines_1, labels_1 = ax1.get_legend_handles_labels()
lines_2, labels_2 = ax2.get_legend_handles_labels()
ax1.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper left')

plt.title('Throughput and CWND over Time')
plt.savefig(args.dir+'/throughput_cwnd.pdf', dpi=1000, bbox_inches='tight')
