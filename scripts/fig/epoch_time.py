import matplotlib.pyplot as plt
import custom_style
from custom_style import remove_chart_junk
import sys
import numpy as np
import math
import util

out_name = sys.argv[2]
in_name = sys.argv[1]
print(("Out: %s") % (out_name))
print(("In: %s") % (in_name))
block_sz = 160

data = util.parseDataNew2(in_name)

throughput = []
latency = []

for elem in data:
    if len(throughput) > 0 and (elem["throughput"] < throughput[len(throughput) - 1]):
        continue
    throughput.append(elem["throughput"])
    latency.append(elem["mean_latency"])

print(throughput)
print(latency)

fig = plt.figure(figsize = (8,8))
ax = fig.add_subplot(111)
ax.plot(throughput, latency, color=custom_style.hash_colors[0])
ax.set_xlabel("Throughput (reqs/sec)")
ax.set_ylabel("Latency (ms)")
#ax.set_yticks([100 * (2 ** 20), 200 * (2 ** 20), 300 * (2 ** 20), 400 * (2 ** 20), 500 * (2 ** 20)])
#ax.set_yticklabels(["100MB", "200MB", "300MB", "400MB", "500MB"])

remove_chart_junk(plt,ax)
custom_style.save_fig(fig, out_name, [3, 2.5])
