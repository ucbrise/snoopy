import re
import custom_style
from custom_style import setup_columns,col,remove_chart_junk
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter
import math
from collections import defaultdict
from matplotlib.patches import Patch
import scipy.special
from scipy.special import lambertw
from scale_breakdown import makeBreakdownFig

import argparse
from util import parse_args

parser = argparse.ArgumentParser(description='Plot scale breakdown.')
args = parse_args(parser)
in_name, out_name = args.input, args.output

print("update")
makeBreakdownFig(in_name, out_name, 2**15, "$2^{15}$ objects", args)
