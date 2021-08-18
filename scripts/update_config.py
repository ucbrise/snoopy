import argparse
import json

from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description='Update config with oblix baselines or new machines')
    parser.add_argument('-m', '--machine_file',
                        help='update machines for an experiment')
    parser.add_argument('-o', '--oblix_dir', type=str, help='oblix directory')
    parser.add_argument('config', type=str, help='config to update')
    args = parser.parse_args()

    with open(args.config, "r") as f:
        config = json.load(f)
    
    if args.oblix_dir:
        oblix_file = Path(args.oblix_dir) / "final_results.json"
        with open(oblix_file, "r") as f:
            oblix_latency = json.load(f)
        config["oblix_baseline"] = oblix_latency
    
    if args.machine_file:
        with open(args.machine_file, "r") as f:
            machines = json.load(f)
        config["machines"] = machines
        config.pop("client_ips", None)
        config.pop("client_private_ips", None)
        config.pop("balancer_ips", None)
        config.pop("balancer_private_ips", None)
        config.pop("suboram_ips", None)
        config.pop("suboram_private_ips", None)

    with open(args.config, "w") as f:
        json.dump(config, f, indent=2)

if __name__ == '__main__':
    main()