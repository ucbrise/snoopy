import re
import custom_style
from custom_style import setup_columns,col,remove_chart_junk
import matplotlib.pyplot as plt 
import sys 
import numpy as np
from matplotlib.ticker import FuncFormatter
import math
from collections import defaultdict
from matplotlib.patches import Patch
import pylab

out_name = sys.argv[2]
in_name = sys.argv[1]

legend_labels= ["Load balancer\n(make batch)", "SubORAM\n(process batch)", "Load balancer\n(match responses)"]
colors=["#FFCA3E","#FF6F50","#D03454"]

fig = plt.figure(figsize=(10,8))

legend_elements = []
for i in range(len(legend_labels)):
    legend_elements.append(Patch(color=colors[i], label=legend_labels[i]))

fig.legend(bbox_to_anchor=(0, 0.0, 1, 1), mode='expand',
    handles=legend_elements, fontsize=8, ncol=3, loc="center", handlelength=1.5, borderpad=0.2,
    borderaxespad=0, labelspacing=1, columnspacing=1, handletextpad=0.7)

custom_style.save_fig(fig, out_name, [3.3, 0.3])
plt.savefig("temp.pdf")

