import argparse
import json

from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description='Generate baselines.json')
    parser.add_argument('obladi_dir', type=str, help='obladi directory')
    parser.add_argument('oblix_dir', type=str, help='oblix directory')
    parser.add_argument('out_file', type=str, help='output json file')
    args = parser.parse_args()

    results = {}
    obladi_file = Path(args.obladi_dir) / "results.dat"
    with open(obladi_file, "r") as f:
        for line in f.readlines():
            if not line:
                continue
            obladi_througput = int(line.strip().split(" ")[-1])
    results["obladi_throughput"] = obladi_througput

    oblix_file = Path(args.oblix_dir) / "final_results.json"
    with open(oblix_file, "r") as f:
        oblix_latency = json.load(f)
    results["oblix_latency"] = oblix_latency

    with open(args.out_file, "w") as f:
        json.dump(results, f, indent=2)
    

if __name__ == '__main__':
    main()