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

colors = ["#62BEB6", "#0B9A8D", "#077368", "#ED5C8B"]

thread1 = []
thread2 = []
thread4 = []
smart = []
blocks = [2**i for i in range(10,17)]

with open(in_name, "r") as f:
    lines = f.readlines()
    for line in lines:
        times = line.split()
        thread1.append(float(times[0]))
        thread2.append(float(times[1]))
        thread4.append(float(times[2]))
        smart.append(float(times[3]))

fig = plt.figure(figsize = (8,8))
ax = fig.add_subplot(111)
ax.plot(blocks, thread1, color=config.pink_sequence_colors[0], label="1 thread", marker="o")
ax.plot(blocks, thread2, color=config.pink_sequence_colors[1], label="2 threads", marker="o")
ax.plot(blocks, thread4, color=config.pink_sequence_colors[2], label="4 threads", marker="o")
ax.plot(blocks, smart, color=config.green_sequence_colors[2], label="Adaptive", marker="^")
ax.set_xlabel("Objects")
ax.set_ylabel("Sort Time (ms)")
ax.set_xscale("log")
ax.set_yscale("log")
#plt.legend(bbox_to_anchor=(-0.3, 1.0, 1., -.102), loc='lower left', ncol=2, borderaxespad=0., fontsize=7,labelspacing=0)
#plt.legend()
ax.set_xticks([2**10, 2**12, 2**14, 2**16])
ax.set_xticklabels(["$2^{10}$", "$2^{12}$", "$2^{14}$", "$2^{16}$"])
ax.set_yticks([1,5,10,50,100])
ax.set_yticklabels(["1","5","10","50","100"])

remove_chart_junk(plt,ax, grid=True)
if args.title:
    ax.set_title(args.title, y=1.5)
if args.large:
    plt.legend(bbox_to_anchor=(0,1.02,1,0.2), loc="lower left",
            mode="expand", borderaxespad=0)
    custom_style.save_fig(fig, out_name, [2.5, 4], pad=0.3)
else:
    custom_style.save_fig(fig, out_name, [1.8, 1.5])
plt.savefig("temp.pdf")

