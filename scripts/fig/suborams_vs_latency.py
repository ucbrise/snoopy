import argparse
import matplotlib.pyplot as plt
import custom_style
from custom_style import remove_chart_junk
import sys
import numpy as np
import math
import util
import config

from util import parse_args
parser = argparse.ArgumentParser(description='Plot suborams vs. latency.')
args = parse_args(parser)
in_name, out_name = args.input, args.output
print(("Out: %s") % (out_name))
print(("In: %s") % (in_name))
block_sz = 160

num_blocks = 2000000 
latency_type = "mean_latency"
'''
# ./app osm insert-one 1 3000000
oblix_insert_latency = 0.021087882518768312
#./app osm search 1 3000000 1
oblix_search_latency = 0.014064238071441651
# ./app osm delete-one 1 3000000'
oblix_delete_latency = 0.0
oblix_latency =  (oblix_insert_latency + (oblix_search_latency + oblix_delete_latency)) / 2.0
'''
TIME_MS = 1
oblix_latency = (0.001146628141*1000) / TIME_MS
obladi_latency = 149 / TIME_MS

data = util.parseDataNew2(in_name)
suborams = util.getListOfVals(data, "suborams")
latencies = []
latencies_50 = []
latencies_75 = []
latencies_90 = []
latencies_95 = []
oblix_latencies = []
obladi_latencies = []
for suboram in suborams:
    latencies.append(util.getLatencyForSuboramAndDataSize(data, suboram, num_blocks, latency_type) / TIME_MS)
    latencies_50.append(util.getLatencyForSuboramAndDataSize(data, suboram, num_blocks, "50_latency") / TIME_MS)
    latencies_75.append(util.getLatencyForSuboramAndDataSize(data, suboram, num_blocks, "75_latency") / TIME_MS)
    latencies_90.append(util.getLatencyForSuboramAndDataSize(data, suboram, num_blocks, "90_latency") / TIME_MS)
    latencies_95.append(util.getLatencyForSuboramAndDataSize(data, suboram, num_blocks, "95_latency") / TIME_MS)
    oblix_latencies.append(oblix_latency)
    obladi_latencies.append(obladi_latency)

print(suborams)
print(latencies)

fig = plt.figure(figsize = (8,8))
ax = fig.add_subplot(111)
ax.plot(suborams, latencies, color=config.dumbo_color, label="Snoopy", marker=config.dumbo_marker)
#ax.plot(suborams, latencies_50, color=colors[0], label="Dumbo (50th percentile)")
#ax.plot(suborams, latencies_75, color=colors[1], label="Dumbo (75th percentile)")
#ax.plot(suborams, latencies_90, color=colors[2], label="Dumbo (90th percentile)")
#ax.plot(suborams, latencies_95, color=colors[3], label="Dumbo (95th percentile)")
ax.plot(suborams, oblix_latencies, color=config.oblix_color, label="Oblix (1 machine)", linestyle=config.oblix_line)
ax.plot(suborams, obladi_latencies, color=config.obladi_color, label="Obladi (2 machines)", linestyle=config.obladi_line)
ax.set_xlabel("SubORAMs")
ax.set_ylabel("Average latency (ms)")
#ax.set_yticks(range(0,11, 2))
ax.spines['bottom'].set_position("zero")
#ax.set_yticks([20 * (2 ** 20), 40 * (2 ** 20), 60 * (2 ** 20), 80 * (2 ** 20), 100 * (2 ** 20)])
#ax.set_yticklabels(["20MB", "40MB", "60MB", "80MB", "100MB"])
#plt.legend()


remove_chart_junk(plt,ax,grid=True)
if args.title:
    ax.set_title(args.title)
if args.large:
    plt.legend()
    custom_style.save_fig(fig, out_name, [2.5, 2], pad=0.3)
else:
    custom_style.save_fig(fig, out_name, [1.8, 1.5])
plt.savefig("temp.pdf")
