import util
import custom_style
from custom_style import remove_chart_junk
import sys
import matplotlib.pyplot as plt
import config

out_name = sys.argv[2]
in_name = sys.argv[1]

colors = ["#3A63AD", "#3BB58F","#3AA5D1"]

def plot_overheads(secparam=128):
    """
    x-axis: # total real requests received by LB
    y-axis: # total requests sent out by LB
    lines: ideal + varying secparam values
    """
    fig = plt.figure(figsize=(8,8))
    #ax = fig.add_subplot(111)
    ax = fig.add_axes([0.25, 0.21, 0.7, 0.65])
    suborams = [1, 10, 20]
    targets = list(range(100, 10001, 100))
    for i, n_suborams in enumerate(suborams):
        overhead = []
        for target in targets:
            bound = util.f(target, n_suborams, secparam)
            total_req = bound * n_suborams
            overhead.append(100 * (total_req - target) / target)
        label = "%d" % n_suborams
        ax.plot(targets, overhead, label=label, color=config.blue_sequence_colors[i])
    ax.set_xlabel("Real requests")
    ax.set_ylabel(r"%% Overhead ($\lambda=%d$)" % secparam)
    ax.set_ylim(bottom=0, top=200)
    ax.set_xticks([0,5000,10000])
    ax.set_xticklabels(["0", "5K", "10K"])
    leg = fig.legend(bbox_to_anchor=(0.01, 0.91, 0.98, .1), ncol=3, loc='lower left', fontsize=8, handlelength=1.25, borderpad=0.2,
            borderaxespad=0, labelspacing=0, columnspacing=0.5, handletextpad=0.25,
            mode='expand', title="\# SubORAMs:", title_fontsize=8)
    custom_style.legend_title_left(leg)
    #plt.legend(bbox_to_anchor=(0.1, 0.75, 1., -.102), loc='lower left', ncol=1, borderaxespad=0., fontsize=7,labelspacing=0)
    remove_chart_junk(plt,ax)
    custom_style.save_fig(fig, out_name, [1.6,1.4])
    plt.savefig("temp.pdf")

plot_overheads()
