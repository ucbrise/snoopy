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

max_latencies = [100,200,300,400,10000]

data = util.parseDataNew2(in_name)
balancers = util.getListOfVals(data, "balancers")
actual_balancers = []
throughput = []
oblix_throughputs = []
oblix_balancers = []
for max_latency in max_latencies:
    throughput.append([])
    actual_balancers.append([])
    index = len(throughput) - 1
    for balancer in balancers:
        val = util.getMaxThroughputForNumBalancersWithMaxLatency(data, balancer, max_latency)
        if val > 0:
            throughput[index].append(val)
            actual_balancers[index].append(balancer)

for balancer in balancers:
    oblix_throughputs.append(oblix_throughput)
    oblix_balancers.append(balancer)
print(balancers)
print(throughput)

fig = plt.figure(figsize = (8,8))
ax = fig.add_subplot(111)
for i in range(len(throughput)):
    ax.plot(actual_balancers[i], throughput[i], label=("Dumbo (max latency=%d ms)") % (max_latencies[i]))
ax.plot(oblix_balancers, oblix_throughputs, color=custom_style.hash_colors[1], label="Oblix (1 machine)")
ax.set_xlabel("Load balancers")
ax.set_ylabel("Throughput (reqs/sec)")
#ax.set_yticks([20 * (2 ** 20), 40 * (2 ** 20), 60 * (2 ** 20), 80 * (2 ** 20), 100 * (2 ** 20)])
#ax.set_yticklabels(["20MB", "40MB", "60MB", "80MB", "100MB"])
#plt.legend()

#ax.spines['left'].set_position("zero")
ax.spines['bottom'].set_position("zero")

print("HI")

remove_chart_junk(plt,ax)
custom_style.save_fig(fig, out_name, [3, 2.5])
