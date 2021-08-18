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

reqs = defaultdict(list)
micros = defaultdict(list)
suborams = set()
with open(in_name, 'r') as f:
    lines = f.readlines()
    for line in lines:
        arr = line.split()
        suboram = int(arr[0])
        suborams.add(suboram)
        reqs[suboram].append(int(arr[1]))
        micros[suboram].append(float(int(arr[2])) / 1000.0)

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
ax.plot(reqs[1], micros[1], color=custom_style.hash_colors[0], marker='o')
ax.set_xlabel("Requests")
ax.set_ylabel("Time (ms)")
#ax.set_yticks([20 * (2 ** 20), 40 * (2 ** 20), 60 * (2 ** 20), 80 * (2 ** 20), 100 * (2 ** 20)])
#ax.set_yticklabels(["20MB", "40MB", "60MB", "80MB", "100MB"])

remove_chart_junk(plt,ax)
custom_style.save_fig(fig, out_name, [3, 2.5])
