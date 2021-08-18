import argparse
import re


from pathlib import Path, PurePath

from util_py3.prop_util import *
from util_py3.ssh_util import executeCommandWithOutputReturn, executeRemoteCommand, getFile

BALANCER_CONFIG_FILE = "config/balancer_benchmark.config"

PLACEHOLDER_SUBORAM_ADDR = "1.1.1.1:12345"

OUTPUT_RE = re.compile(r'\d+ \d+ \d+')

def bench(mode):
    props = loadPropertyFile(BALANCER_CONFIG_FILE)
    results = []
    for suborams in range(1,11):
        props["mode"] = mode
        props["protocol"] = "our_protocol"
        props["suboram_addrs"] = [[PLACEHOLDER_SUBORAM_ADDR] for i in range(suborams)]
        with open(BALANCER_CONFIG_FILE, 'w') as fp:
            json.dump(props, fp, indent = 2, sort_keys=True)
        output = executeCommandWithOutputReturn("../build/load_balancer/host/load_balancer_host ../build/load_balancer/enc/load_balancer_enc.signed %s" % BALANCER_CONFIG_FILE, printCmd=False)
        #print(output)
            
        for line in str(output, "utf-8").split('\n'):
            m = OUTPUT_RE.match(line)
            if m:
                print(line)
                '''
                    batch_sz = int(m.group(1))
                    time_ms = int(m.group(2)) / 1e3
                    results.append((n, batch_sz, time_ms))
                    print("%d %d %.2f" % (n, batch_sz, time_ms))
        for n, batch_sz, time_ms in results:
            print("%d %d %.2f" % (n, batch_sz, time_ms))
        '''


def main():
    parser = argparse.ArgumentParser(description='Run load balancer benchmark.')
    parser.add_argument('-b', '--make_batch', action='store_true',
                        help='bench make_batch (default: false)')
    parser.add_argument('-r', '--match_responses', action='store_true',
                        help='bench match_responses (default: false)')
    args = parser.parse_args()
    if args.make_batch:
        bench("bench_make_batch")
    elif args.match_responses:
        bench("bench_match_resps")
    else:
        parser.print_help()
        parser.exit()


def runExperiment(propFile):
    properties = loadPropertyFile(propFile)
    machines = properties["machines"]
    balancerIp = machines["balancer_ips"][0]
    username = properties["username"]
    host = f"{username}@{balancerIp}"
    sshKey = properties["ssh_key_file"]
    experimentType = properties["experiment_type"]

    localProjectDir = properties['local_project_dir']
    expDir = properties['experiment_dir']
    # Project dir on the remote machine
    remoteProjectDir = properties['remote_project_dir']
    remoteExpDir = PurePath(remoteProjectDir, expDir)
    localExpDir = Path(localProjectDir, expDir)

    if experimentType == "bench_make_batch":
        cmd = f"cd snoopy/scripts; python3 balancer_benchmark.py -b 1> {remoteExpDir / 'final_results.dat'}"
    elif experimentType == "bench_match_responses":
        cmd = f"cd snoopy/scripts; python3 balancer_benchmark.py -r 1> {remoteExpDir / 'final_results.dat'}"
    executeRemoteCommand(host, cmd, key=sshKey, printfn=lambda x: x)
    getFile(localExpDir, [balancerIp], remoteExpDir / 'final_results.dat', key=sshKey)



if __name__ == '__main__':
    main()
