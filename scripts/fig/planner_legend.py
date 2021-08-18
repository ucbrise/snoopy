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
from matplotlib.lines import Line2D
import pylab
import config

out_name = sys.argv[2]
in_name = sys.argv[1]

labels = ["1M objects", "10K objects"]
markers = ["o","^"]

fig = plt.figure(figsize=(10,8))

legend_elements = []
for i in range(len(labels)):
    legend_elements.append(Line2D([0],[0],color='w',markerfacecolor=config.planner_colors[i], label=labels[i], marker=markers[i], markersize=7))

legend_elements.reverse()

fig.legend(handles=legend_elements, fontsize=8, ncol=3, loc="center", labelspacing=0)

custom_style.save_fig(fig, out_name, [5.0, 1.0])

