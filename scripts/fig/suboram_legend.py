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

legend_labels= ["Snoopy", "Obladi (2 machines)", "Oblix (1 machine)"]
colors = [config.dumbo_color, config.obladi_color, config.oblix_color]
markers = ["o", "None", "None"]
linestyles = ["-", config.obladi_line, config.oblix_line]

fig = plt.figure(figsize=(10,8))

legend_elements = []
for i in range(len(legend_labels)):
    legend_elements.append(Line2D([0],[0],color=colors[i], label=legend_labels[i], marker=markers[i], linestyle=linestyles[i]))

fig.legend(bbox_to_anchor=(0, 0.0, 1, 1), mode='expand',
    handles=legend_elements, fontsize=8, ncol=3, loc="center", handlelength=1.5, borderpad=0.2,
    borderaxespad=0, labelspacing=1, columnspacing=1, handletextpad=0.7)
#fig.legend(handles=legend_elements, fontsize=8, ncol=3, loc="center", labelspacing=0, columnspacing=0.8, handlelength=1.5, handletextpad=0.5)

custom_style.save_fig(fig, out_name, [3.3, 0.3])
plt.savefig("temp.pdf")
