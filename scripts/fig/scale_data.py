import matplotlib.pyplot as plt
import custom_style
from custom_style import remove_chart_junk
import sys
import numpy as np
import math
import util
import config

import argparse
from util import parse_args

parser = argparse.ArgumentParser(description='Plot suborams vs. data size.')
args = parse_args(parser)
in_name, out_name = args.input, args.output
print(("Out: %s") % (out_name))
print(("In: %s") % (in_name))
block_sz = 160

max_latency_ms = 160
latency_types = ["mean_latency"]

data = util.parseDataNew2(in_name)
suborams = sorted(util.getListOfVals(data, "suborams"))
datasize = []
for latency_type in latency_types:
    datasize.append([])
    index = len(datasize) - 1
    for suboram in suborams:
        datasize[index].append(util.getMaxDataForNumSuborams(data, suboram, max_latency_ms, latency_type))

print(suborams)
print(datasize)

fig = plt.figure(figsize = (8,8))
ax = fig.add_subplot(111)
for i in range(len(latency_types)):
    ax.plot(suborams, datasize[i], color=config.dumbo_color, marker=config.dumbo_marker, label="Snoopy")
ax.set_xlabel("SubORAMs")
ax.set_ylabel("Number of objects")
#ax.set_yticks([0.5 * (10 ** 6), 1 * (10 ** 6), 1.5 * (10 ** 6), 2 * (10 ** 6), 2.5*10**6, 3*10**6])
#ax.set_yticklabels(["0.5M", "1M", "1.5M", "2M", "2.5M", "3M"])
ax.set_yticks([0, 1e6, 2e6, 3e6])
ax.set_yticklabels(["0", "1M", "2M", "3M"])
#plt.legend()

remove_chart_junk(plt,ax, grid=True)
if args.title:
    ax.set_title(args.title)
if args.large:
    plt.legend()
    custom_style.save_fig(fig, out_name, [2.5, 2], pad=0.3)
else:
    custom_style.save_fig(fig, out_name, [1.8, 1.5])
plt.savefig("temp.pdf")
