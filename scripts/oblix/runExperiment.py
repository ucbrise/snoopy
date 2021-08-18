import argparse
import datetime
import queue
import sys

from pathlib import Path, PurePath

sys.path.append('..')

from util_py3.azure_util import *
from util_py3.prop_util import *
from util_py3.ssh_util import *

az = AzureSetup("oblix_baseline")

MACHINES_FILE = "config/machines.json"

def provisionExperiment(propFile, machinesFile):
    properties = loadPropertyFile(propFile)
    machines = loadPropertyFile(machinesFile)
    #az.cleanupAzure()
    compute_client, network_client = az.runAzureSetup(properties["location"])

    oblix_result_queue = queue.Queue()
    threads = []
    total_machines = properties["num_instances"]["oblix"]
    pbar = tqdm(total=total_machines*3, desc="[Provisioning resources for VMs...]")
    threads.extend(
        az.startAzureInstancesAsync(
            compute_client, network_client, "oblix",
            properties["instance_types"]["oblix"], properties["location"],
            properties["image_path"], properties["ssh_key_pub"],
            properties["num_instances"]["oblix"],
            oblix_result_queue,
            pbar,
        )
    )
    for t in threads:
        t.join()
    pbar.close()
    
    oblix_vms = []
    oblix_ips = []
    oblix_private_ips = []
    for _ in range(properties["num_instances"]["oblix"]):
        vm_poller, ip, private_ip = oblix_result_queue.get()
        oblix_vms.append(vm_poller)
        oblix_ips.append(ip)
        oblix_private_ips.append(private_ip)

    machines["oblix_ips"] = oblix_ips
    machines["oblix_private_ips"] = oblix_ips
    properties["machines"] = machines
    with open(propFile, 'w') as fp:
        json.dump(properties, fp, indent=2, sort_keys=True)
    with open(machinesFile, 'w') as fp:
        json.dump(machines, fp, indent=2, sort_keys=True)
 
    all_vms = oblix_vms
    print("Waiting for VMs to finish starting...")
    for vm in tqdm(all_vms):
        vm.result()

    return oblix_vms

def cleanupExperiment():
    az.cleanupAzure()

def setupExperiment(propFile):
    properties = loadPropertyFile(propFile)
    # Username for ssh-ing.
    username = properties['username']
    # Name of the experiment that will be run
    experimentName = properties['experiment_name']
    # Project dir on the local machine
    localProjectDir = PurePath(properties['local_project_dir'])
    # Project dir on the remote machine
    remoteProjectDir = properties['remote_project_dir']
    # Source directory on the local machine (for compilation)
    localSrcDir = properties['local_src_dir']
    sshKeyFile = properties["ssh_key_file"]
    machines = properties["machines"]

    # The experiment folder is generated with the following path:
    # results/experimentName/Date
    # The date is used to distinguish multiple runs of the same experiment
    expFolder = PurePath('results', experimentName)
    expDir = expFolder / datetime.datetime.now().strftime("%Y:%m:%d:%H:%M")
    properties['experiment_dir'] = str(expDir)

    # LocalPath and RemotePath describe where data will be stored on the remote machine
    # and on the local machine
    localPath = str(localProjectDir / expDir)
    remotePath = str(remoteProjectDir / expDir)

    executeCommand("mkdir -p %s" % localPath)
    tmpSymlink = "tmp_symlink"
    symlinkDir = os.path.join(localProjectDir, "results", experimentName, "latest")
    os.symlink(localPath, tmpSymlink)
    os.rename(tmpSymlink, symlinkDir)

    # Generate exp directory on all machines
    all_ips = machines["oblix_ips"]
    mkdirRemoteHosts(properties["username"], all_ips, remotePath, properties["ssh_key_file"])

    with open(propFile, 'w') as fp:
        json.dump(properties, fp, indent = 2, sort_keys=True)

def runExperiment(propFile):
    properties = loadPropertyFile(propFile)
    machines = properties["machines"]
    oblixIp = machines["oblix_ips"][0]
    username = properties["username"]
    host = f"{username}@{oblixIp}"
    sshKey = properties["ssh_key_file"]

    localProjectDir = properties['local_project_dir']
    expDir = properties['experiment_dir']
    # Project dir on the remote machine
    remoteProjectDir = properties['remote_project_dir']
    remoteExpDir = PurePath(remoteProjectDir, expDir)
    localExpDir = Path(localProjectDir, expDir)
    
    total_experiments = len(properties['nbBlockSizes']) * len(properties['nbBlocks'])
    experiment_bar = tqdm(total=total_experiments, position=0, desc="[Experiment progress]")
    for block_sz in properties['nbBlockSizes']:
        for num_blocks in properties['nbBlocks']:
            round_file = f"{block_sz}_{num_blocks}"
            cmd = f"cd ~/osheep/experiments/dosm-microbenchmarks/bin; ./app oram access {num_blocks} {block_sz} | grep 'time' > {remoteExpDir/round_file}"
            executeRemoteCommand(host, cmd, key=sshKey, printfn=tqdm.write)
            experiment_bar.update(1)
    threads = getDirectory(localExpDir.parent, username, [oblixIp], remoteExpDir, key=sshKey)
    for t in threads:
        t.join()


def summarizeData(propFile):
    properties = loadPropertyFile(propFile)
    localProjectDir = properties['local_project_dir']
    expDir = properties['experiment_dir']
    localExpDir = Path(localProjectDir, expDir)
    results = {}
    for block_sz in properties['nbBlockSizes']:
        results[block_sz] = {}
    for block_sz in properties['nbBlockSizes']:
        for num_blocks in properties['nbBlocks']:
            round_file = f"{block_sz}_{num_blocks}"
            with open(localExpDir/round_file, 'r') as f:
                for l in f.readlines():
                    if not l:
                        continue
                    time = float(l.strip().split(": ")[-1])
            results[block_sz][num_blocks] = time
    with open(localExpDir/"final_results.json", "w+") as f:
        json.dump(results, f, indent=2)


def main():
    parser = argparse.ArgumentParser(description='Run experiment.')
    parser.add_argument('-p', '--provision', action='store_true',
                        help='provision instances (default: false)')
    parser.add_argument('-s', '--setup', action='store_true',
                        help='setup instances (default: false)')
    parser.add_argument('-r', '--run', action='store_true',
                        help='run experiment (default: false)')
    parser.add_argument('--summarize', action='store_true',
                        help='summarize experiment results (default: false)')
    parser.add_argument('-c', '--cleanup', action='store_true',
                        help='cleanup instances (default: false)')
    parser.add_argument('--config', default='config/oblix_benchmark.json', help='config file for experiment (default: config/oblix_benchmark.json)')
    args = parser.parse_args()

    if not args.cleanup and not args.config:
        parser.print_help()
        parser.exit()

    if not args.provision and not args.setup and not args.run and not args.summarize and not args.cleanup:
        parser.print_help()
        parser.exit()

    config_file = args.config
    if args.provision:
        print("Provisioning...")
        provisionExperiment(config_file, MACHINES_FILE)
    if args.setup:
        print("Setting up...")
        setupExperiment(config_file)
    if args.run:
        print("Running experiment...")
        runExperiment(config_file)
        summarizeData(config_file)
    if args.summarize:
        summarizeData(config_file)
    if args.cleanup:
        print("Cleaning up... this will take a few minutes.")
        cleanupExperiment()

if __name__ == '__main__':
    main()