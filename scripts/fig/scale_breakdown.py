import matplotlib
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
import scipy.special
from scipy.special import lambertw

lb_1_name = "micro_balancer_make_batch.dat"
lb_2_name = "micro_balancer_match_resps.dat"
suboram_name = "micro_suboram_batch_sz.dat"

labels = ["Load balancer (make batch)", "SubORAM (process batch)", "Load balancer (match responses)"] 
#colors=[custom_style.mix_colors[2], custom_style.hash_colors[4], custom_style.hash_colors[1], custom_style.hash_colors[0]]
colors=["#FFCA3E","#FF6F50","#D03454"]

suborams = 1
data_size = 2**10


def getLoadBalancerData(filename):
    results = []
    f1 = open(filename, "r")
    lines_1 = f1.readlines()
    for i in range(len(lines_1)):
        elems_1 = lines_1[i].split()
        result = { 
            "suborams": int(elems_1[0]),
            "requests": int(elems_1[1]),
            "latency": (float(elems_1[2])) / 1000000.0,
            }   
        results.append(result)
    f1.close()
    return results

def getSuboramData():
    results = []
    with open(suboram_name, "r") as f:
        lines = f.readlines()
        for line in lines:
            elems = line.split()
            result = { 
                "data_size": int(elems[0]),
                "batch": int(elems[1]),
                "latency": float(elems[2]) / 1000.0,
                }   
            results.append(result)
    return results

def f(N, n_suborams, secparam=128):
    mu = N / n_suborams
    alpha = math.log(n_suborams * (2 ** secparam))
    rhs = alpha / (math.e * mu) - 1 / math.e
    branch = 0
    epsilon = math.e ** (lambertw(rhs, branch) + 1) - 1
    #epsilon = (alpha + math.sqrt(2 * mu * alpha)) / mu     # uncomment for looser bound
    #print(alpha, rhs, lambertw(rhs, 0), lambertw(rhs, 1))
    #print("bound", suborams, secparam, alpha, rhs, lambertw(rhs), epsilon)
    return mu * (1 + epsilon)

def getLoadBalancerLatencyForParams(data, suborams, requests):
    for elem in data:
        if elem["suborams"] == suborams and elem["requests"] == requests:
            return elem["latency"]
    print(("load balancer out-of-bounds params: no latency for params suborams=%d, requests=%d") % (suborams, requests))
    return -1.0

def getSuboramLatencyForParams(data, data_size, batch):
    for elem in data:
        if elem["data_size"] == data_size and elem["batch"] == batch:
            return elem["latency"]
    print(("suboram out-of-bounds params: no latency for params data_size=%d, batch=%d") % (data_size, batch))
    return -1.0

def roundUpPow2(x):
    return 2 ** (math.ceil(math.log(x,2)))

def makeBreakdownFig(in_name, out_name, data_size, title, args):
    lb_1_data = getLoadBalancerData(lb_1_name)
    lb_2_data = getLoadBalancerData(lb_2_name)
    suboram_data = getSuboramData()
    lb1_plt = []
    lb2_plt = []
    suboram_plt = []
    reqs_plt = [2**i for i in range(6,11)]

    for reqs in reqs_plt:
        lb1_plt.append(getLoadBalancerLatencyForParams(lb_1_data,suborams,reqs) * 1000)
        lb2_plt.append(getLoadBalancerLatencyForParams(lb_2_data,suborams,reqs) * 1000)
        batch_size_rounded = roundUpPow2(f(reqs,suborams))
        suboram_plt.append(getSuboramLatencyForParams(suboram_data,data_size,reqs) * 1000)

    fig = plt.figure(figsize = (8,8))
    ax = fig.add_subplot(111)
    ax.stackplot(reqs_plt, lb1_plt, suboram_plt, lb2_plt, labels=labels, colors=colors)
    #ax.stackplot(np.arange(10, 110, step=10), y[0], y[1], y[2], y[3], labels=labels, colors=colors)
    ax.set_xlabel("Requests")
    ax.set_ylabel("Process time (ms)")
    #ax.set_yscale('log')
    ax.set_xscale('log')
    ax.set_xticks([2**6, 2**8, 2**10])
    ax.set_xticklabels(["$2^6$", "$2^8$", "$2^{10}$"])
    #ax.set_title(title, fontsize=8)

    print("updated")
    #plt.legend()

    #ax.spines['left'].set_position("zero")
    #ax.spines['bottom'].set_position("zero")
    remove_chart_junk(plt,ax,lightGrid=True,below=False)

    pgf_with_pdflatex = {
        "pgf.texsystem": "pdflatex",
        "pgf.preamble": [
            r"""
            %        \input{../fonts}
            \usepackage[T1]{fontenc}
            \newcommand\hmmax{0}
            \usepackage{amsmath}
            \usepackage{amssymb}
            \usepackage{mathptmx}
            """,
            ],
        "text.usetex": True,
        "font.family": "serif",
        "font.serif": [],
        "font.sans-serif": [],
        "font.monospace": [],
        "axes.labelsize": 7,
        "font.size": 10,
        "legend.fontsize": 7,
        "xtick.labelsize": 7,
        "ytick.labelsize": 7,
        "lines.markersize": 3,
        "lines.markeredgewidth": 0,
        "axes.linewidth": 0.5,
        }


    matplotlib.rcParams.update(pgf_with_pdflatex)

    #ax.yaxis.grid(which='major', color='0.9', linestyle='dotted')
    if args.title:
        ax.set_title(args.title, y=1.5)
    if args.large:
        plt.legend(bbox_to_anchor=(0,1.02,1,0.2), loc="lower left",
                mode="expand", borderaxespad=0)
        custom_style.save_fig(fig, out_name, [2.5, 3], pad=0.3)
    else:
        custom_style.save_fig(fig, out_name, [1.3, 1.4])
    #custom_style.save_fig(fig, out_name, [3.25, 1.8])
    #plt.show()
