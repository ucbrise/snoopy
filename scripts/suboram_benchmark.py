import argparse
from pathlib import Path, PurePath
import re

#sys.path.append("util/")

from util_py3.prop_util import *
from util_py3.ssh_util import executeCommandWithOutputReturn, executeRemoteCommand, getFile

SUBORAM_CONFIG_FILE = "config/suboram_benchmark.config"

OUTPUT_RE = re.compile(r'.*total blocks, (\d+) batch size: (\d+)\.')
SORT_OUTPUT_RE = re.compile(r'.*_sort.* time for \d+ total blocks: (\d+)\.')


def bench_sort(sort_type, num_threads):
    props = loadPropertyFile(SUBORAM_CONFIG_FILE)
    results = []
    for i in range(10, 17):
        n = 2**i
        props["num_blocks"] = n
        props["mode"] = "bench_sort"
        props["protocol"] = "our_protocol"
        props["sort_type"] = sort_type
        props["threads"] = num_threads
        with open(SUBORAM_CONFIG_FILE, 'w') as fp:
            json.dump(props, fp, indent = 2, sort_keys=True)
        output = executeCommandWithOutputReturn("../build/suboram/host/suboram_host ../build/suboram/enc/suboram_enc.signed %s" % SUBORAM_CONFIG_FILE, printCmd=False)
        for line in str(output, 'utf-8').split('\n'):
            m = SORT_OUTPUT_RE.match(line)
            if m:
                time_ms = int(m.group(1)) / 1e3
                results.append((n, time_ms))
                #print("%d,%.2f" % (n, time_ms))
    for n, time_ms in results:
        print("%d,%.2f" % (n, time_ms))


def bench_process_batch(num_threads):
    props = loadPropertyFile(SUBORAM_CONFIG_FILE)
    results = []
    for i in range(10, 22):
        n = 2**i
        props["num_blocks"] = n
        props["mode"] = "bench_process_batch"
        props["protocol"] = "our_protocol"
        props["threads"] = num_threads
        with open(SUBORAM_CONFIG_FILE, 'w') as fp:
            json.dump(props, fp, indent = 2, sort_keys=True)
        output = executeCommandWithOutputReturn("../build/suboram/host/suboram_host ../build/suboram/enc/suboram_enc.signed %s" % SUBORAM_CONFIG_FILE, printCmd=False)
        for line in str(output, 'utf-8').split('\n'):
            m = OUTPUT_RE.match(line)
            if m:
                batch_sz = int(m.group(1))
                time_ms = int(m.group(2)) / 1e3
                results.append((n, batch_sz, time_ms))
                #print("%d %d %.2f" % (n, batch_sz, time_ms))
    for n, batch_sz, time_ms in results:
        print("%d %d %.2f" % (n, batch_sz, time_ms))


def main():
    parser = argparse.ArgumentParser(description='Run suboram benchmark.')
    parser.add_argument('-t', '--threads', type=int, default=3,
                        help='num threads (default: false)')
    parser.add_argument('-s', '--sort', action='store_true',
                        help='bench sort (default: false)')
    parser.add_argument('-a', '--adaptive-sort', action='store_true',
                        help='bench adaptive sort (default: false)')
    parser.add_argument('-p', '--process_batch', action='store_true',
                        help='bench process_batch (default: false)')
    args = parser.parse_args()
    if args.sort:
        bench_sort("bitonic_sort_nonadaptive", args.threads)
    elif args.adaptive_sort:
        bench_sort("bitonic_sort", args.threads)
    elif args.process_batch:
        bench_process_batch(args.threads)
    else:
        parser.print_help()
        parser.exit()
    #bench_sort("bucket_sort")
    #bench_sort("buffered_bucket_sort")


def runExperiment(propFile):
    properties = loadPropertyFile(propFile)
    machines = properties["machines"]
    suboramIp = machines["suboram_ips"][0][0]
    username = properties["username"]
    host = f"{username}@{suboramIp}"
    sshKey = properties["ssh_key_file"]
    experimentType = properties["experiment_type"]

    localProjectDir = properties['local_project_dir']
    expDir = properties['experiment_dir']
    # Project dir on the remote machine
    remoteProjectDir = properties['remote_project_dir']
    remoteExpDir = PurePath(remoteProjectDir, expDir)
    localExpDir = Path(localProjectDir, expDir)

    if experimentType == "bench_sort":
        cmd = f"cd snoopy/scripts; python3 suboram_benchmark.py -s -t 1 > {remoteExpDir / '1.dat' }"
        executeRemoteCommand(host, cmd, key=sshKey)
        getFile(localExpDir, [suboramIp], remoteExpDir / '1.dat', key=sshKey)

        cmd = f"cd snoopy/scripts; python3 suboram_benchmark.py -s -t 2 > {remoteExpDir / '2.dat' }"
        executeRemoteCommand(host, cmd, key=sshKey)
        getFile(localExpDir, [suboramIp], remoteExpDir / '2.dat', key=sshKey)

        cmd = f"cd snoopy/scripts; python3 suboram_benchmark.py -s -t 3 > {remoteExpDir / '3.dat' }"
        executeRemoteCommand(host, cmd, key=sshKey)
        getFile(localExpDir, [suboramIp], remoteExpDir / '3.dat', key=sshKey)

        cmd = f"cd snoopy/scripts; python3 suboram_benchmark.py -a -t 3 > {remoteExpDir / 'adaptive.dat' }"
        executeRemoteCommand(host, cmd, key=sshKey)
        getFile(localExpDir, [suboramIp], remoteExpDir / 'adaptive.dat', key=sshKey)

        one_thread = []
        with open(localExpDir / '1.dat') as f:
            for l in f.readlines():
                one_thread.append(l.split(",")[1].strip())
        two_threads = []
        with open(localExpDir / '2.dat') as f:
            for l in f.readlines():
                two_threads.append(l.split(",")[1].strip())
        three_threads = []
        with open(localExpDir / '3.dat') as f:
            for l in f.readlines():
                three_threads.append(l.split(",")[1].strip())
        adaptive = []
        with open(localExpDir / 'adaptive.dat') as f:
            for l in f.readlines():
                adaptive.append(l.split(",")[1].rstrip())
        with open(localExpDir / 'final_results.dat', 'w') as f:
            for one, two, three, adaptive in zip(one_thread, two_threads, three_threads, adaptive):
                print(f"{one} {two} {three} {adaptive}", file=f)
    elif experimentType == "bench_process_batch_threads":
        cmd = f"cd snoopy/scripts; python3 suboram_benchmark.py -p -t 1 > {remoteExpDir / '1.dat' }"
        executeRemoteCommand(host, cmd, key=sshKey)
        getFile(localExpDir, [suboramIp], remoteExpDir / '1.dat', key=sshKey)

        cmd = f"cd snoopy/scripts; python3 suboram_benchmark.py -p -t 2 > {remoteExpDir / '2.dat' }"
        executeRemoteCommand(host, cmd, key=sshKey)
        getFile(localExpDir, [suboramIp], remoteExpDir / '2.dat', key=sshKey)

        cmd = f"cd snoopy/scripts; python3 suboram_benchmark.py -p -t 3 > {remoteExpDir / '3.dat' }"
        executeRemoteCommand(host, cmd, key=sshKey)
        getFile(localExpDir, [suboramIp], remoteExpDir / '3.dat', key=sshKey)

        one_thread = []
        with open(localExpDir / '1.dat') as f:
            for l in f.readlines():
                n, batch_sz, ms = l.split(" ")
                if batch_sz == "4096":
                    one_thread.append(ms.strip())
        two_threads = []
        with open(localExpDir / '2.dat') as f:
            for l in f.readlines():
                n, batch_sz, ms = l.split(" ")
                if batch_sz == "4096":
                    two_threads.append(ms.strip())
        three_threads = []
        with open(localExpDir / '3.dat') as f:
            for l in f.readlines():
                n, batch_sz, ms = l.split(" ")
                if batch_sz == "4096":
                    three_threads.append(ms.strip())
        with open(localExpDir / 'final_results.dat', 'w') as f:
            for one, two, three in zip(one_thread, two_threads, three_threads):
                print(f"{one} {two} {three}", file=f)
    elif experimentType == "bench_process_batch":
        cmd = f"cd snoopy/scripts; python3 suboram_benchmark.py -p > {remoteExpDir / 'final_results.dat' }"
        executeRemoteCommand(host, cmd, key=sshKey)
        getFile(localExpDir, [suboramIp], remoteExpDir / 'final_results.dat', key=sshKey)


if __name__ == '__main__':
    main()
