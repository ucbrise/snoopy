import matplotlib.pyplot as plt
import custom_style
from custom_style import remove_chart_junk
from collections import defaultdict
import sys
import numpy as np
import math
import util
import numpy as np
import config

import argparse
from util import parse_args

parser = argparse.ArgumentParser(description='Plot suborams vs. data size.')
args = parse_args(parser)
in_name, out_name = args.input, args.output
print(("Out: %s") % (out_name))
print(("In: %s") % (in_name))
block_sz = 160

params = ["config1", "config3", "config2"]
colors = ["#E65F8E", "#A86BD1", "#3AA5D1"]
#labels = ["$2^{25}$ blocks", "$2^{20}$ blocks", "$2^{15}$ blocks"]
labels = ["1M objects", "10K objects", "placeholder"]
markers = ["o","*","^"]

cost = defaultdict(list)
throughput = defaultdict(list)
with open(in_name, 'r') as f:
    lines = f.readlines()
    for line in lines:
        arr = line.split()
        _cost = float(arr[2])
        if _cost < 4043:
            param = arr[0]
            throughput[param].append(float(arr[1]))
            cost[param].append(float(arr[2]))

throughput_filtered = throughput
cost_filtered = cost
'''
throughput_filtered = defaultdict(list)
cost_filtered = defaultdict(list)
for param in params:
    indexes = [0, len(cost[param]) - 1]
    indexes = sorted(indexes + [math.floor(i * len(cost[param]) / 5) for i in range(1,5)])
    for index in indexes:
        cost_filtered[param].append(cost[param][index])
        throughput_filtered[param].append(throughput[param][index])
'''

fig = plt.figure(figsize = (8,8))
ax = fig.add_subplot(111)

index = 0
'''
for suboram in suborams:
    ax.plot(reqs[suboram], micros[suboram], color=custom_style.hash_colors[index], label=("%d suborams") % (suboram))
    print(suboram)
    print(reqs[suboram])
    print(micros[suboram])
'''
for i, param in enumerate(params):
    s = [x/1500.0 + 10 for x in throughput_filtered[param]]
    if throughput_filtered[param]:
        ax.scatter(throughput_filtered[param], cost_filtered[param], color=config.planner_colors[i], label=labels[i], marker=config.planner_markers[i],s=s)
    #plt.plot(np.unique(throughput[param]), np.poly1d(np.polyfit(throughput[param], cost[param], 1))(np.unique(throughput[param])), color=colors[i])
ax.set_xlabel("Throughput (reqs/sec)")
ax.set_ylabel("Cost per month (\$)")
ax.set_xlim(-5000, 140000)
ax.set_xticks([0,40000, 80000, 120000])
ax.xaxis.set_tick_params(width=0.5)
ax.set_xticklabels(["0","40K", "80K", "120K"])
ax.set_yticks([1000,2000,3000,4000])
ax.set_yticklabels(["\$1K", "\$2K","\$3K", "\$4K"])
#plt.legend()

remove_chart_junk(plt,ax,xticks=False)
if args.title:
    plt.title(args.title, y=1.3)
    #ax.set_title(args.title)
if args.large:
    plt.legend()
    custom_style.save_fig(fig, out_name, [3, 2.5], pad=0.3)
else:
    custom_style.save_fig(fig, out_name, [1.6, 1.5])
plt.savefig("temp.pdf")
