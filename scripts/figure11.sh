#!/bin/bash
set -x

python3 runExperiment.py -sr --config config/balancerMakeBatch.json
python3 runExperiment.py -sr --config config/balancerMatchResponses.json
python3 runExperiment.py -sr --config config/suboramProcessBatch.json

cp ../experiments/results/balancer_match_responses/latest/final_results.dat ../experiments/results/balancer_make_batch/latest/micro_balancer_match_resps.dat
cp ../experiments/results/balancer_make_batch/latest/final_results.dat ../experiments/results/balancer_make_batch/latest/micro_balancer_make_batch.dat
cp ../experiments/results/suboram_process_batch/latest/final_results.dat ../experiments/results/balancer_make_batch/latest/micro_suboram_batch_sz.dat

cp fig/scale_breakdown*.py fig/custom_style.py fig/tufte.py fig/util.py ../experiments/results/balancer_make_batch/latest

cd ../experiments/results/balancer_make_batch/latest/
python3 scale_breakdown_1024.py -l -t "Artifact Evaluation" 1024 ../../../artifact_figures/artifact/11a.pdf
python3 scale_breakdown_32768.py -l -t "Artifact Evaluation" 32768 ../../../artifact_figures/artifact/11b.pdf
python3 scale_breakdown_1048576.py -l -t "Artifact Evaluation" 1048576 ../../../artifact_figures/artifact/11c.pdf

cd ../../../../scripts/fig
python3 scale_breakdown_1024.py -l -t "Paper Figure 11a" 1024 ../../experiments/artifact_figures/paper/11a.pdf
python3 scale_breakdown_32768.py -l -t "Paper Figure 11b" 32768 ../../experiments/artifact_figures/paper/11b.pdf
python3 scale_breakdown_1048576.py -l -t "Paper Figure 11c" 1048576 ../../experiments/artifact_figures/paper/11c.pdf

cd ../../experiments/artifact_figures
pdfjam --landscape --nup 2x1 artifact/11a.pdf paper/11a.pdf --outfile 11a.pdf
pdfjam --landscape --nup 2x1 artifact/11b.pdf paper/11b.pdf --outfile 11b.pdf
pdfjam --landscape --nup 2x1 artifact/11c.pdf paper/11c.pdf --outfile 11c.pdf
