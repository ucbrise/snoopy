import matplotlib.pyplot as plt
import custom_style
from custom_style import remove_chart_junk
from collections import defaultdict
import sys
import numpy as np
import math
import util

out_name = sys.argv[2]
in_name = sys.argv[1]

print(("Out: %s") % (out_name))
print(("In: %s") % (in_name))
block_sz = 160

algs = ["hash_table", "bucket_sort"]
colors = [custom_style.hash_colors[0], custom_style.hash_colors[1]]
labels = ["Hash table", "Bucket sort"]

blocks = defaultdict(list)
ms = defaultdict(list)
with open(in_name, 'r') as f:
    lines = f.readlines()
    for line in lines:
        arr = line.split()
        alg = arr[0]
        blocks[alg].append(int(arr[1]))
        ms[alg].append(float(arr[2]))

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
for i, alg in enumerate(algs):
    ax.plot(blocks[alg], ms[alg], color=colors[i], label=labels[i], marker='o')
ax.set_xlabel("Blocks")
ax.set_ylabel("Time (ms)")
#ax.set_xticks([500000, 1000000, 1500000])
#ax.set_xticklabels(["500K", "1M", "1.5M"])
plt.legend()

remove_chart_junk(plt,ax)
custom_style.save_fig(fig, out_name, [3, 2.5])
