import argparse
import brewer2mpl
import tufte
import matplotlib

matplotlib.use("pgf")
pgf_with_pdflatex = {
    "pgf.texsystem": "pdflatex",
    "pgf.preamble":
         r"""
%        \input{../fonts}
    \usepackage[T1]{fontenc}
    \newcommand\hmmax{0}
    \usepackage{amsmath}
    \usepackage{amssymb}
    \usepackage{mathptmx}
         """,
    "text.usetex": True,
    "font.family": "serif",
    "font.serif": [], 
    "font.sans-serif": [],
    "font.monospace": [],
    "axes.labelsize": 8, 
    "font.size": 10,
    "legend.fontsize": 8, 
    "xtick.labelsize": 8,
    "ytick.labelsize": 8,
    "lines.markersize": 3, 
    "lines.markeredgewidth": 0,
    "axes.linewidth": 0.5,
}


matplotlib.rcParams.update(pgf_with_pdflatex)

import matplotlib.style
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

_markers = ["o", "v", "s", "*", "D", "^"]
hash_markers = _markers
mix_markers = _markers

def megabytes(x, pos):
  """Formatter for Y axis, values are in megabytes"""
  if x < 1024:
      return '%d B' % (x)
  elif x < 1024 * 1024:
      return '%1.0f KiB' % (x/1024)
  else:
      return '%1.0f MiB' % (x/(1024*1024))

# brewer2mpl.get_map args: set name  set type  number of colors
bmap1 = brewer2mpl.get_map('Set1', 'Qualitative', 7)
bmap2 = brewer2mpl.get_map('Dark2', 'Qualitative', 7)
hash_colors = bmap1.mpl_colors
mix_colors = bmap2.mpl_colors
fig, ax = plt.subplots()
tufte.tuftestyle(ax)
plt.tight_layout()
plt.grid(axis='y', color="0.9", linestyle='-', linewidth=1)

def save_fig(fig, out_name, size=[3.15,1.5], pad=0):
    fig.set_size_inches(size)
    fig.tight_layout()
    plt.savefig(out_name, dpi=600, bbox_inches='tight', pad_inches = pad)
  
def setup_columns(f):
    return f.readline().split()

def col(pieces, cols, name):
    return pieces[cols.index(name)]

def remove_chart_junk(plt, ax, grid=False, xticks=False, ticks=False, below=True, lightGrid=False):
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.get_xaxis().tick_bottom()
    ax.get_yaxis().tick_left()
    if not xticks:
        ax.xaxis.set_ticks_position('none')
    if not ticks:
        ax.yaxis.set_ticks_position('none')
    else:
        plt.minorticks_off()

    ax.set_axisbelow(below)
    if grid:
        #plt.grid(b=True, which='major', color='0.9', linestyle='-')
    #else:
        ax.yaxis.grid(which='major', color='0.9')
    if lightGrid:
        ax.yaxis.grid(which='major', color='0.9', linestyle=':')


def legend_title_left(leg):
    c = leg.get_children()[0]
    title = c.get_children()[0]
    hpack = c.get_children()[1]
    c._children = [hpack]
    hpack._children = [title] + hpack.get_children()

densely_dashed = (0, (3, 1))
densely_dotted = (0, (1, 1))
