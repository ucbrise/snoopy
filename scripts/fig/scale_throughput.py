import matplotlib.pyplot as plt
import matplotlib.text as mtext
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

'''
# ./app osm insert-one 1 100000
oblix_insert_latency=0.014399878978729248
#./app osm search 1 100000 1
oblix_search_latency=0.00820251226425171
oblix_avg_latency = (oblix_insert_latency + oblix_search_latency) / 2.0
oblix_throughput = 1.0 / oblix_avg_latency
'''
baseline = util.parse_baseline(args.baseline)
#oblix_latency = 0.0008669505119
#obladi_throughput = 6716
oblix_latency = baseline["oblix_latency"]["160"]["2000000"]
oblix_throughput = 1.0 / oblix_latency
obladi_throughput = baseline["obladi_throughput"]
# obladi throughput for 2M blocks

data_1000 = util.parseDataNew2(in_name)
data_500 = util.parseDataNew2(in_name+'_500')
data_300 = util.parseDataNew2(in_name+'_300')

from collections import defaultdict
oblix_throughputs = []
obladi_throughputs = []
def get_machines_and_throughput(data, max_latency):
    throughput = defaultdict(float)
    alloc = {}
    suborams = util.getListOfVals(data, "suborams")
    balancers = util.getListOfVals(data, "balancers")
    for suboram, balancer in util.getTupleListOfVals(data, "suborams", "balancers"):
        print(suboram, balancer)
        total_machines = suboram + balancer
        xput = util.getMaxThroughputForNumBalancersWithMaxMeanLatency(data, balancer, max_latency, suborams=suboram)
        if xput > throughput[total_machines]:
            throughput[total_machines] = xput
            alloc[total_machines] = (suboram, balancer)
    cores = []
    machines = []
    max_throughputs = []
    print(alloc)
    for i in sorted(throughput.keys()):
        if i >= 19 or i <= 3:
            continue
        cores.append(i*4)
        machines.append(i)
        max_throughputs.append(throughput[i])
    print(machines)
    print(max_throughputs)
    return machines, max_throughputs

machines_1000, throughput_1000 = get_machines_and_throughput(data_1000, 1000)
machines_500, throughput_500 = get_machines_and_throughput(data_500, 500)
machines_300, throughput_300 = get_machines_and_throughput(data_300, 300)

plt.tight_layout()
fig = plt.figure(figsize = (12,10))
#ax = fig.add_subplot(111)
ax = fig.add_axes([0.2, 0.18, 0.75, 0.625])
machines = [i for i in range(2, 6)] + machines_1000 + [machines_1000[-1]+1]
t_obladi, = ax.plot(machines, [obladi_throughput] * len(machines), color=config.obladi_color, label="2 machines, 0.xs", linestyle=custom_style.densely_dashed)
machines = [1] + machines
t_oblix, = ax.plot(machines, [oblix_throughput] * len(machines), color=config.oblix_color, label="Oblix (1 machine)", linestyle=custom_style.densely_dotted)
t_1000, = ax.plot(machines_1000, throughput_1000, color=config.green_sequence_colors[0], label="1s", marker=config.dumbo_marker)
t_500, = ax.plot(machines_500, throughput_500, color=config.green_sequence_colors[1], label="0.5s", marker=config.dumbo_marker)
t_300, = ax.plot(machines_300, throughput_300, color=config.green_sequence_colors[2], label="0.3s", marker=config.dumbo_marker)
ax.set_xlabel("Machines")
ax.set_ylabel("Throughput (reqs/sec)")
ax.set_yticks([50000,100000])
ax.set_xlim(3, 18.3)
ax.set_yticklabels(["50K", "100K"])
#ax.set_yticks([20 * (2 ** 20), 40 * (2 ** 20), 60 * (2 ** 20), 80 * (2 ** 20), 100 * (2 ** 20)])
#ax.set_yticklabels(["20MB", "40MB", "60MB", "80MB", "100MB"])

#ax.spines['left'].set_position("zero")
ax.spines['bottom'].set_position("zero")
#plt.legend()
#plt.legend(bbox_to_anchor=(-0.3, 1.1, 1., -.102), loc='lower left', ncol=2, borderaxespad=0., fontsize=8,labelspacing=0)

p5, = ax.plot([0], marker='None',
           linestyle='None', label='dummy-tophead')
leg = fig.legend([t_1000, t_500, t_300], ['Snoopy 1s', 'Snoopy 0.5s', 'Snoopy 0.3s'],
    bbox_to_anchor=(0.025, 0.91, 0.95, .1), ncol=3, loc='lower left', fontsize=8, handlelength=1.25, borderpad=0.2,
    borderaxespad=0, labelspacing=0, columnspacing=0.5, handletextpad=0.25,
    mode='expand')
leg.get_frame().set_linewidth(0)

leg = fig.legend([t_obladi, t_oblix], ['Obladi (2 machines)', 'Oblix (1 machine)'],
    bbox_to_anchor=(0.025, 0.82, 0.95, .1), ncol=2, loc='lower left', fontsize=8, handlelength=1.25, borderpad=0.2,
    borderaxespad=0, labelspacing=0, columnspacing=0.5, handletextpad=0.25,
    mode='expand')
leg.get_frame().set_linewidth(0)

fig.patches.extend([plt.Rectangle((0.025,0.82), 0.95, 0.17,
                      fill=False, edgecolor='gray', linewidth=0.4, zorder=1000,
                      transform=fig.transFigure, figure=fig)])

remove_chart_junk(plt,ax,grid=True)

if args.title:
    plt.title(args.title, y=1.3)
    #ax.set_title(args.title)
if args.large:
    custom_style.save_fig(fig, out_name, [3, 2.5], pad=0.3)
else:
    custom_style.save_fig(fig, out_name, [2.4, 2])
plt.savefig("temp.pdf")

