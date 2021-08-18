#!/bin/bash
set -x

cd ../planner

cp ../experiments/results/balancer_make_batch/latest/micro_*.dat bench

python3 plannerBenchmark.py

cd ../scripts
python3 fig/planner_config.py -l -t "Artifact Evaluation" ../planner/scale_throughput_config.dat ../experiments/artifact_figures/artifact/14a.pdf
python3 fig/planner_cost.py -l -t "Artifact Evaluation" ../planner/scale_throughput_cost.dat ../experiments/artifact_figures/artifact/14b.pdf

python3 fig/planner_config.py -l -t "Paper Figure 14a" ../experiments/artifact_figures/paper/planner_config.dat ../experiments/artifact_figures/paper/14a.pdf
python3 fig/planner_cost.py -l -t "Paper Figure 14b" ../experiments/artifact_figures/paper/planner_cost.dat ../experiments/artifact_figures/paper/14b.pdf

cd ../experiments/artifact_figures
pdfjam --landscape --nup 2x1 artifact/14a.pdf paper/14a.pdf --outfile 14a.pdf
pdfjam --landscape --nup 2x1 artifact/14b.pdf paper/14b.pdf --outfile 14b.pdf
