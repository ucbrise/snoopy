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

'''
# ./app osm insert-one 1 100000
oblix_insert_latency=0.014399878978729248
#./app osm search 1 100000 1
oblix_search_latency=0.00820251226425171
oblix_avg_latency = (oblix_insert_latency + oblix_search_latency) / 2.0
oblix_throughput = 1.0 / oblix_avg_latency
'''
oblix_latency = 0.0008669505119
oblix_throughput = 1.0 / oblix_latency

data = util.parseDataNew2(in_name)
balancers = util.getListOfVals(data, "balancers")
latency = []
oblix_latencies = []
for balancer in balancers:
    print(balancer)
    latency.append(util.getLatencyForMaxThroughputForNumBalancers(data, balancer))
    oblix_latencies.append(oblix_latency)


print(balancers)
print(latency)

fig = plt.figure(figsize = (8,8))
ax = fig.add_subplot(111)
ax.plot(balancers, latency, color=custom_style.hash_colors[0], label="Dumbo")
ax.plot(balancers, oblix_latencies, color=custom_style.hash_colors[1], label="Oblix (1 machine)")
print("plotted")
ax.set_xlabel("Load balancers")
ax.set_ylabel("Latency (ms)")
#ax.set_yticks([20 * (2 ** 20), 40 * (2 ** 20), 60 * (2 ** 20), 80 * (2 ** 20), 100 * (2 ** 20)])
#ax.set_yticklabels(["20MB", "40MB", "60MB", "80MB", "100MB"])
plt.legend()

#ax.spines['left'].set_position("zero")
#ax.spines['bottom'].set_position("zero")

remove_chart_junk(plt,ax)
custom_style.save_fig(fig, out_name, [3, 2.5])
