import util
import custom_style
from custom_style import remove_chart_junk
import sys
import matplotlib.pyplot as plt

out_name = sys.argv[2]
in_name = sys.argv[1]

def plot_bounds_varying_N(n_suborams=25):
    """
    x-axis: # total real requests received by LB
    y-axis: # total requests sent out by LB
    lines: ideal + varying secparam values
    """
    secparams = [32, 64, 128]
    fig = plt.figure(figsize=(8,8))
    ax = fig.add_subplot(111)
    targets = list(range(100, 50001, 100))
    ax.plot(targets, targets, label="Ideal (no security)")
    for secparam in secparams:
        totals = []
        for target in targets:
            bound = util.f(target, n_suborams, secparam)
            totals.append(bound * n_suborams)
        ax.plot(targets, totals, label="Actual + Dummy (secparam=%d)" % secparam)
    ax.set_xlabel("Total real requests (N)")
    ax.set_ylabel("Total requests (f(N) * S, S=%d)" % n_suborams)
    plt.legend()
    remove_chart_junk(plt,ax)
    custom_style.save_fig(fig, out_name, [3, 2.5])
    plt.savefig("temp.pdf")

plot_bounds_varying_N(25)
