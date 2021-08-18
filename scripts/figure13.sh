#!/bin/bash
set -x

python3 runExperiment.py --config="config/adaptiveScaleThroughputOblix.json" -sr
python3 runExperiment.py --config="config/adaptiveScaleThroughputOblix_500.json" -sr
python3 runExperiment.py --config="config/adaptiveScaleThroughputOblix_300.json" -sr

cp ../experiments/results/oblix_adaptive_scale_throughput_500/latest/final_results.dat ../experiments/results/oblix_adaptive_scale_throughput_1000/latest/final_results.dat_500
cp ../experiments/results/oblix_adaptive_scale_throughput_300/latest/final_results.dat ../experiments/results/oblix_adaptive_scale_throughput_1000/latest/final_results.dat_300

python3 runExperiment.py -g -f 13
