#!/bin/bash
set -x

python3 runExperiment.py -sr -f 9_1000
python3 runExperiment.py -sr -f 9_500
python3 runExperiment.py -sr -f 9_300

cp ../experiments/results/adaptive_scale_throughput_500/latest/final_results.dat ../experiments/results/adaptive_scale_throughput_1000/latest/final_results.dat_500
cp ../experiments/results/adaptive_scale_throughput_300/latest/final_results.dat ../experiments/results/adaptive_scale_throughput_1000/latest/final_results.dat_300

python3 runExperiment.py -g -f 9
