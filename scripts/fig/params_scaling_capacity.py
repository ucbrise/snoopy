import util
import custom_style
from custom_style import remove_chart_junk
import sys
import matplotlib.pyplot as plt
import config

out_name = sys.argv[2]
in_name = sys.argv[1]
colors = ["#A86BD1","#3AA5D1","#E65F8E"]

def plot_scaling_capacity(batchsize=1000):
    """
    x-axis: # total real requests received by LB
    y-axis: # total requests sent out by LB
    lines: ideal + varying secparam values
    """
    secparams = [80, 128]
    fig = plt.figure(figsize=(8,8))
    ax = fig.add_axes([0.25, 0.21, 0.7, 0.65])
    suborams = list(range(1, 21))
    theoretical = [batchsize * s for s in suborams]
    ax.plot(suborams, theoretical, label="0 (no security)", color=config.purple_sequence_colors[0])
    for i,secparam in enumerate(secparams):
        total_reqs = []
        for n_suborams in suborams:
            total_req = util.max_requests(n_suborams, batchsize, secparam)
            total_reqs.append(total_req)
        ax.plot(suborams, total_reqs, label="%d$" % secparam,color=config.purple_sequence_colors[i+1])
    ax.set_xlabel("SubORAMs")
    ax.set_ylabel("Real request capacity")
    ax.set_yticks([5000,10000,15000,20000])
    ax.set_yticklabels(["5K", "10K", "15K", "20K"])
    ax.set_xticks([0,10,20])
    leg = fig.legend(bbox_to_anchor=(0.01, 0.91, 0.98, .1), ncol=3, loc='lower left', fontsize=8, handlelength=1, borderpad=0.2,
            borderaxespad=0, labelspacing=0, columnspacing=0.5, handletextpad=0.25,
            mode='expand', title="$\lambda$:", title_fontsize=8)
    custom_style.legend_title_left(leg)
    #plt.legend(bbox_to_anchor=(0.1, 1.0, 1., -.102), loc='lower left', ncol=1, borderaxespad=0., fontsize=7,labelspacing=0)
    #plt.legend(bbox_to_anchor=(0.1, 0.75, 1., -.102), loc='lower left', ncol=1, borderaxespad=0., fontsize=7,labelspacing=0)
    #plt.legend()
    remove_chart_junk(plt,ax)
    custom_style.save_fig(fig, out_name, [1.6, 1.4])
    plt.savefig("temp.pdf")

plot_scaling_capacity(1000)
