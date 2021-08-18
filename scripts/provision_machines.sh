#!/bin/bash
set -x

python3 runExperiment.py -p -f 9

python3 update_config.py -m="config/machines.json" config/adaptiveScaleThroughput_300.json
python3 update_config.py -m="config/machines.json" config/adaptiveScaleThroughput_500.json
python3 update_config.py -m="config/machines.json" config/adaptiveScaleThroughputOblix.json
python3 update_config.py -m="config/machines.json" config/scaleData.json
python3 update_config.py -m="config/machines.json" config/scaleDataLatency.json
python3 update_config.py -m="config/machines.json" config/balancerMakeBatch.json
python3 update_config.py -m="config/machines.json" config/balancerMatchResponses.json
python3 update_config.py -m="config/machines.json" config/suboramProcessBatch.json