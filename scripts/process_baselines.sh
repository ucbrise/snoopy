#!/bin/bash
set -x

python3 get_baselines.py ../experiments/results/diff-clients-base-oram-par-server-1/latest ../experiments/results/oblix_benchmark/latest ../experiments/artifact_figures/artifact/baselines.json

python3 update_config.py -o="../experiments/results/oblix_benchmark/latest" config/adaptiveScaleThroughputOblix.json