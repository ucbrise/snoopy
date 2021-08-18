import util
import custom_style
from custom_style import remove_chart_junk
import sys
import matplotlib.pyplot as plt

out_name = sys.argv[2]
in_name = sys.argv[1]

def plot_alphas(n_suborams=10):
    """
    x-axis: # total real requests received by LB
    y-axis: # total requests sent out by LB
    lines: ideal + varying secparam values
    """
    secparams = [32, 64, 128]
    fig = plt.figure(figsize=(8,8))
    ax = fig.add_subplot(111)
    targets = list(range(100, 50001, 100))
    ax.plot(targets, [1/n_suborams] * len(targets), label="Ideal (no security)")
    ax.plot(targets, [1] * len(targets), label="Naive (replicate to all subORAMs)")
    for secparam in secparams:
        alphas = []
        for target in targets:
            bound = util.f(target, n_suborams, secparam)
            alphas.append(bound / target)
        ax.plot(targets, alphas, label=r"Our bound ($\lambda=%d$)" % secparam)
    ax.set_xlabel("Total real requests (N)")
    ax.set_ylabel(r"Value of $\alpha$ in $f(N) = \alpha\cdot N$, S=%d" % n_suborams)
    plt.legend()
    remove_chart_junk(plt,ax)
    custom_style.save_fig(fig, out_name, [3, 2.5])

plot_alphas(10)
