import argparse
import io
import os
import os.path
import sys
import datetime
import time
import math
import queue

from util_py3.ssh_util import *
from util_py3.azure_util import *
from util_py3.prop_util import *
from util_py3.math_util import *
from util_py3.graph_util import *

import suboram_benchmark
import balancer_benchmark

from tqdm import tqdm, trange
from pathlib import Path, PurePath

clientPrefix = "client-"
balancerPrefix = "balancer-"
suboramPref = "suboram-"

CLIENT_CONFIG_FILE = "config/client.config"
BALANCER_CONFIG_FILE = "config/lb.config"
SUBORAM_CONFIG_FILE = "config/suboram.config"
SSH_CONFIG_FILE = "config/ssh_config"
MACHINES_FILE = "config/machines.json"

az = AzureSetup("scalable_oram_throughput")

def figure_to_config_file(figure):
    if figure == "9":
        return "config/adaptiveScaleThroughput.json"
    if figure == "9_1000":
        return "config/adaptiveScaleThroughput.json"
    if figure == "9_500":
        return "config/adaptiveScaleThroughput_500.json"
    if figure == "9_300":
        return "config/adaptiveScaleThroughput_300.json"
    if figure == "10a":
        return "config/scaleData.json"
    if figure == "10b":
        return "config/scaleDataLatency.json"
    if figure == "12a":
        return "config/suboramSort.json"
    if figure == "12b":
        return "config/suboramProcessBatchThreads.json"
    if figure == "13":
        return "config/adaptiveScaleThroughputOblix.json"

def provisionExperiment(propFile, machinesFile):
    properties = loadPropertyFile(propFile)
    machines = loadPropertyFile(machinesFile)
    #az.cleanupAzure()
    compute_client, network_client = az.runAzureSetup(properties["location"])

    client_result_queue = queue.Queue()
    balancer_result_queue = queue.Queue()
    suboram_result_queue = [queue.Queue() for _ in range(properties["num_instances"]["suboram"])]
    threads = []
    total_machines = properties["num_instances"]["suboram"]*properties["replication_factor"] + properties["num_instances"]["client"] + properties["num_instances"]["balancer"]
    pbar = tqdm(total=total_machines*3, desc="[Provisioning resources for VMs...]")
    threads.extend(
        az.startAzureInstancesAsync(
            compute_client, network_client, "client",
            properties["instance_types"]["client"], properties["location"],
            properties["image_path"], properties["ssh_key_pub"],
            properties["num_instances"]["client"],
            client_result_queue,
            pbar,
        )
    )
    threads.extend(
        az.startAzureInstancesAsync(
            compute_client, network_client, "balancer",
            properties["instance_types"]["balancer"], properties["location"],
            properties["image_path"], properties["ssh_key_pub"],
            properties["num_instances"]["balancer"],
            balancer_result_queue,
            pbar,
        )
    )
    for i in range(properties["num_instances"]["suboram"]):
        threads.extend(
            az.startAzureInstancesAsync(
                compute_client, network_client, ("suboram%d-") % (i),
                properties["instance_types"]["suboram"], properties["location"],
                properties["image_path"], properties["ssh_key_pub"],
                properties["replication_factor"],
                suboram_result_queue[i],
                pbar,
            )
        )

    for t in threads:
        t.join()
    pbar.close()
    
    client_vms = []
    client_ips = []
    client_private_ips = []
    for _ in range(properties["num_instances"]["client"]):
        vm_poller, ip, private_ip = client_result_queue.get()
        client_vms.append(vm_poller)
        client_ips.append(ip)
        client_private_ips.append(private_ip)

    balancer_vms = []
    balancer_ips = []
    balancer_private_ips = []
    for _ in range(properties["num_instances"]["balancer"]):
        vm_poller, ip, private_ip = balancer_result_queue.get()
        balancer_vms.append(vm_poller)
        balancer_ips.append(ip)
        balancer_private_ips.append(private_ip)
    
    suboram_vms = []
    suboram_ips = []
    suboram_private_ips = []
    for i in range(properties["num_instances"]["suboram"]):
        vms = []
        ips = []
        private_ips = []
        for _ in range(properties["replication_factor"]):
            vm, ip, private_ip = suboram_result_queue[i].get()
            vms.append(vm)
            ips.append(ip)
            private_ips.append(private_ip)
        suboram_vms = suboram_vms + vms
        suboram_ips.append(ips)
        suboram_private_ips.append(private_ips)

    machines["client_ips"] = client_ips
    machines["client_private_ips"] = client_private_ips
    machines["balancer_ips"] = balancer_ips
    machines["balancer_private_ips"] = balancer_private_ips
    machines["suboram_ips"] = suboram_ips
    machines["suboram_private_ips"] = suboram_private_ips
    properties.pop("client_ips", None)
    properties.pop("client_private_ips", None)
    properties.pop("balancer_ips", None)
    properties.pop("balancer_private_ips", None)
    properties.pop("suboram_ips", None)
    properties.pop("suboram_private_ips", None)
    properties["machines"] = machines
    with open(propFile, 'w') as fp:
        json.dump(properties, fp, indent=2, sort_keys=True)
    with open(machinesFile, 'w') as fp:
        json.dump(machines, fp, indent=2, sort_keys=True)
 
    all_vms = client_vms + balancer_vms + suboram_vms
    print("Waiting for VMs to finish starting...")
    for vm in tqdm(all_vms):
        vm.result()

    return client_vms, balancer_vms, suboram_vms

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
    gitOrigin = properties["git_origin"]
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

    # Config files for each
    suboramConfig = loadPropertyFile(SUBORAM_CONFIG_FILE)
    balancerConfig = loadPropertyFile(BALANCER_CONFIG_FILE)
    clientConfig = loadPropertyFile(CLIENT_CONFIG_FILE)

    clientConfig["lb_addrs"] = [addr + ":12345" for addr in machines["balancer_ips"]]
    clientConfig["num_blocks"] = properties["num_blocks"]
    clientConfig["experiment_dir"] = properties["experiment_dir"]
    with open(CLIENT_CONFIG_FILE, 'w') as fp:
        json.dump(clientConfig, fp, indent = 2, sort_keys=True)
 
    balancerConfig["listening_port"] = "12345"
    balancerConfig["threads"] = 1
    balancerConfig["num_blocks"] = properties["num_blocks"]
    balancerConfig["suboram_addrs"] = []
    for ips in machines["suboram_ips"]:
        balancerConfig["suboram_addrs"].append([addr + ":12346" for addr in ips])
    #balancerConfig["suboram_addrs"] = [addr + ":12346" for addr in properties["suboram_ips"]]
    with open(BALANCER_CONFIG_FILE, 'w') as fp:
        json.dump(balancerConfig, fp, indent = 2, sort_keys=True)
 
    suboramConfig["listening_port"] = "12346"
    suboramConfig["num_blocks"] = properties["num_blocks"]
    with open(SUBORAM_CONFIG_FILE, 'w') as fp:
        json.dump(suboramConfig, fp, indent = 2, sort_keys=True)
 
    # Generate exp directory on all machines
    all_ips = machines["client_ips"] + machines["balancer_ips"]
    for suboram_ips in machines["suboram_ips"]:
        all_ips.extend(suboram_ips)
    mkdirRemoteHosts(properties["username"], all_ips, remotePath, properties["ssh_key_file"])

    threads = []
    # Send configs
    for suboramIps in machines["suboram_ips"]:
        threads.extend(sendFileHosts(SUBORAM_CONFIG_FILE, properties["username"], suboramIps, remotePath, properties["ssh_key_file"], join_threads=False))

    threads.extend(sendFileHosts(BALANCER_CONFIG_FILE, properties["username"], machines["balancer_ips"], remotePath, properties["ssh_key_file"], join_threads=False))

    threads.extend(sendFileHosts(CLIENT_CONFIG_FILE, properties["username"], machines["client_ips"], remotePath, properties["ssh_key_file"], join_threads=False))

    for t in tqdm(threads, desc="[Sending config files to hosts]"):
        t.join()


    # Send github ssh keys
    allIps = machines["balancer_ips"] + machines["client_ips"]
    for suboramIps in machines["suboram_ips"]:
        allIps = allIps + suboramIps
    hosts = ["%s@%s" % (username, ip) for ip in allIps]
    sendFileHosts(properties["github_key"], properties["username"], allIps, localSrcDir, properties["ssh_key_file"])
    sendFileHosts(properties["github_pub_key"], properties["username"], allIps, localSrcDir, properties["ssh_key_file"])
    ssh_key_path = os.path.join(localSrcDir, os.path.basename(properties["github_key"]))
    with open(SSH_CONFIG_FILE, 'w') as fp:
        fp.write("Host github.com\n")
        fp.write("  Hostname github.com\n")
        fp.write("  IdentityFile=%s\n" % ssh_key_path)
        fp.write("  StrictHostKeyChecking no\n")
    sendFileHosts(SSH_CONFIG_FILE, properties["username"], allIps, "~/.ssh/config", properties["ssh_key_file"])

    # Pull and build remotely
    housekeepingCmd = "chmod 600 ~/.ssh/config; cd %s; git remote add origin %s; git stash" % (localSrcDir, "foo@bar.com")
    executeParallelBlockingRemoteCommand(hosts, housekeepingCmd, sshKeyFile)

    executeParallelBlockingRemoteCommand(hosts, gitSetOriginCmd(localSrcDir, gitOrigin), sshKeyFile)

    executeParallelBlockingRemoteCommand(hosts, gitPullCmd(localSrcDir), sshKeyFile)

    buildCmd = "cd %s/build; make -j" % localSrcDir
    executeParallelBlockingRemoteCommand(hosts, buildCmd, sshKeyFile)

    with open(propFile, 'w') as fp:
        json.dump(properties, fp, indent = 2, sort_keys=True)

def sleep_bar(duration, desc):
    for _ in trange(duration, desc=f"[{desc}]", leave=False, bar_format='{desc}: {remaining}{postfix}'):
        time.sleep(1)

def wait_until_output(buf, stop_string):
    output = ""
    while True:
        line = buf.getvalue()
        if stop_string in line:
            return
        time.sleep(0.5)
        

def runSingleExperiment(propFile, nbClients, nbSuborams, nbBalancers, nbDataSz, nbBatchSz, nbEpochMs, it):
    properties = loadPropertyFile(propFile)
    username = properties["username"]
    # Project dir on the local machine
    localProjectDir = properties['local_project_dir']
    expDir = properties['experiment_dir']
    # Project dir on the remote machine
    remoteProjectDir = properties['remote_project_dir']
    remoteExpDir = PurePath(remoteProjectDir, expDir)
    localExpDir = Path(localProjectDir, expDir)
    logFolders = properties['log_folder']
    
    sshKey = properties["ssh_key_file"]
    
    machines = properties["machines"]
    clientIpList = machines["client_ips"]
    balancerIpList= machines["balancer_ips"]
    suboramIpList = machines["suboram_ips"]
    balancerIpList= balancerIpList[:nbBalancers]
    suboramIpList = suboramIpList[:nbSuborams]

    if True:
        # Creates underlying file structure: remoteExpDir/1_1 for example for repetition 1 of round 1
        roundName = str(nbClients) + "_" + str(nbDataSz) + "_" + str(nbSuborams) + "_" + str(nbBalancers) + "_" + str(nbBatchSz) + "_" + str(nbEpochMs) + "_" + str(it)
        localRoundFolder = localExpDir / roundName
        remoteRoundFolder = remoteExpDir / roundName
        localPath = localRoundFolder
        remotePath = remoteRoundFolder
        executeCommand("mkdir -p %s" % str(localPath), printfn=tqdm.write)
        logFolder = remotePath / logFolders
        properties['log_folder'] = str(logFolder)
        localProp = localPath / "properties"
        remoteProp = remotePath / "properties"
        executeCommand("mkdir -p %s" % str(localProp), printfn=tqdm.write)
        properties['exp_dir'] = str(remotePath)

        tqdm.write(bordered(
            "Number of clients " + str(nbClients),
            "Data size " + str(nbDataSz),
            "Number of suborams " + str(nbSuborams),
            "Number of load balancers " + str(nbBalancers),
            "Client batch size " + str(nbBatchSz),
            "Epoch ms " + str(nbEpochMs),
        ))
        tqdm.write(bordered(
            "Round Folder: " + str(localRoundFolder),
            "Remote Path: " + str(remotePath),
            "Local properties path: " + str(localProp),
            side_border=False,
        ))

        threads = []
        for c in clientIpList:
            threads.append(executeNonBlockingCommand(mkdirRemoteCmd(username, c, remotePath, sshKey), printfn=tqdm.write, printCmd=False))
            threads.append(executeNonBlockingCommand(mkdirRemoteCmd(username, c, remoteProp, sshKey), printfn=tqdm.write, printCmd=False))
            threads.append(executeNonBlockingCommand(mkdirRemoteCmd(username, c, logFolder, sshKey), printfn=tqdm.write, printCmd=False))

        for lb in balancerIpList:
            threads.append(executeNonBlockingCommand(mkdirRemoteCmd(username, lb, remotePath, sshKey), printfn=tqdm.write, printCmd=False))
            threads.append(executeNonBlockingCommand(mkdirRemoteCmd(username, lb, remoteProp, sshKey), printfn=tqdm.write, printCmd=False))
            threads.append(executeNonBlockingCommand(mkdirRemoteCmd(username, lb, logFolder, sshKey), printfn=tqdm.write, printCmd=False))

        for suboramIps in suboramIpList:
            for s in suboramIps:
                threads.append(executeNonBlockingCommand(mkdirRemoteCmd(username, s, remotePath, sshKey), printfn=tqdm.write, printCmd=False))
                threads.append(executeNonBlockingCommand(mkdirRemoteCmd(username, s, remoteProp, sshKey), printfn=tqdm.write, printCmd=False))
                threads.append(executeNonBlockingCommand(mkdirRemoteCmd(username, s, logFolder, sshKey), printfn=tqdm.write, printCmd=False))
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        # Generate a specific property file for each client/proxy/storage
        tqdm.write(bordered("Starting machines"))
        # Start suborams
        threads = []
        for j in range(nbSuborams):
            #tqdm.write("Starting suboram " + str(j))
            localProp_ = localProp / f"suboram{j}.config"
            remoteProp_ = remoteProp / f"suboram{j}.config"

            suboramConfig = loadPropertyFile(SUBORAM_CONFIG_FILE)
            suboramConfig["run_name"] = str(remotePath / f"{j}_{properties['run_name']}")
            suboramConfig["num_blocks"] = nbDataSz
            suboramConfig["num_balancers"] = nbBalancers
            suboramConfig["num_suborams"] = nbSuborams
            suboramConfig["protocol_type"] = properties["suboram_protocol"]
            suboramConfig["suboram_id"] = j
            suboramConfig["mode"] = "server"
            suboramConfig["sort_type"] = properties["sort_protocol"]
            suboramConfig["threads"] = properties["suboram_threads"]
            #suboramConfig["num_blocks"] = math.ceil(properties["num_blocks"] / nbSuborams)
            #tqdm.write("Going to write local property file " + str(localProp_))
            with open(localProp_, 'w+') as fp:
                json.dump(suboramConfig, fp, indent = 2, sort_keys=True)
            threads.extend(sendFileHosts(localProp_, username, suboramIpList[j], remoteProp_, sshKey, join_threads=False, printCmd=False))
        for t in threads:
            t.join()
        suboram_output = []
        for j in range(nbSuborams):
            suboram_output.append([io.StringIO() for _ in suboramIpList[j]])

        threads = []
        for j in range(nbSuborams):
            for k, suboramIp in enumerate(suboramIpList[j]):
                file_id = suboramIp + "_" + str(j)
                remoteProp_ = remoteProp / f"suboram{j}.config"
                # Already copied the correct config files
                logFile = logFolder / f"suboram_{file_id}.log"
                errLogFile = logFolder / f"suboram_err_{file_id}.log"
                cmd = f"cd snoopy; (./build/suboram/host/suboram_host build/suboram/enc/suboram_enc.signed {remoteProp_} | tee {logFile}) 3>&1 1>&2 2>&3 | tee {errLogFile}"
                t = executeNonBlockingRemoteCommand(username + "@" + suboramIp, cmd, sshKey, printfn=suboram_output[j][k].write)
                t.start()

                t = threading.Thread(target=wait_until_output, args=(suboram_output[j][k], "Started suboram server"))
                t.start()
                threads.append(t)

        for t in tqdm(threads, desc="[Waiting for subORAMs to initialize]", leave=False):
            t.join()
        
        # sleep_bar(10, "Waiting for subORAMs to initialize")

        # Start load balancer
        threads = []
        for j in range(nbBalancers):
            balancerIp = balancerIpList[j]
            #tqdm.write("Starting balancer " + str(j))
            file_id = balancerIp + "_" + str(j)
            localProp_ = localProp / f"balancer{j}.config"
            remoteProp_ = remoteProp / f"balancer{j}.config"

            balancerConfig = loadPropertyFile(BALANCER_CONFIG_FILE)
            balancerConfig["run_name"] = str(remotePath / f'{j}_{properties["run_name"]}')
            balancerConfig["num_blocks"] = nbDataSz
            balancerConfig["epoch_ms"] = nbEpochMs
            balancerConfig["balancer_id"] = j
            balancerConfig["suboram_addrs"] = []
            balancerConfig["mode"] = "server"
            balancerConfig["threads"] = properties["balancer_threads"]
            for k in range(nbSuborams):
                balancerConfig["suboram_addrs"].append([addr + ":12346" for addr in suboramIpList[k]])

            with open(localProp_, 'w+') as fp:
                json.dump(balancerConfig, fp, indent = 2, sort_keys=True)
            threads.extend(sendFileHosts(localProp_, username, [balancerIp], remoteProp_, sshKey, join_threads=False, printCmd=False))
        for t in threads:
            t.join()

        threads = []
        balancer_output = [io.StringIO() for _ in range(nbBalancers)]
        for j in range(nbBalancers):
            balancerIp = balancerIpList[j]
            file_id = balancerIp + "_" + str(j)
            logFile = logFolder / f"balancer_{file_id}.log"
            errLogFile = logFolder / f"balancer_err_{file_id}.log"
            remoteProp_ = remoteProp / f"balancer{j}.config"
            cmd = f"cd snoopy; (./build/load_balancer/host/load_balancer_host build/load_balancer/enc/load_balancer_enc.signed {remoteProp_} | tee {logFile}) 3>&1 1>&2 2>&3 | tee {errLogFile}"
            t = executeNonBlockingRemoteCommand(username + "@" + balancerIp, cmd, sshKey, printfn=balancer_output[j].write)
            t.start()
            t = threading.Thread(target=wait_until_output, args=(balancer_output[j], "Server listening on"))
            t.start()
            threads.append(t)
        for t in tqdm(threads, "[Waiting for load balancers to initialize]", leave=False):
            t.join(20)

        # LATER: need to bring in loader for dataset?
        clientList = list()
        for j in range(nbClients):
            clientIp = clientIpList[j]
            #tqdm.write("Starting client " + str(j))
            file_id = clientIp + "_" + str(j)
            localProp_ = localProp / f"client{j}.config"
            remoteProp_ = remoteProp / f"client{j}.config"

            clientConfig = loadPropertyFile(CLIENT_CONFIG_FILE)
            clientConfig["run_name"] = str(remotePath / f'{j}_{properties["run_name"]}')
            clientConfig["experiment_dir"] = str(remoteRoundFolder);
            clientConfig["num_blocks"] = nbDataSz
            clientConfig["exp_sec"] = properties["exp_sec"]
            clientConfig["threads"] = properties["threads"]
            clientConfig["batch_size"] = nbBatchSz
            clientConfig["num_balancers"] = nbBalancers
            clientConfig["ip_addr"] = clientIp
            clientConfig["client_id"] = j
            with open(localProp_, 'w+') as fp:
                json.dump(clientConfig, fp, indent = 2, sort_keys=True)
            sendFile(localProp_, clientIp, username, remoteProp_, sshKey, printfn=tqdm.write, printCmd=False)

            # Already copied the correct config files
            logFile = logFolder / f"client_{file_id}.log"
            errLogFile = logFolder / f"client_err_{file_id}.log"
            cmd = f"cd snoopy; ./build/client/client {remoteProp_} 1> {logFile} 2> {errLogFile}"
            t = executeNonBlockingRemoteCommand(username + "@" + clientIp, cmd, sshKey, printfn=tqdm.write)
            clientList.append(t)
            # client needs to output tx count, latency pairs
        for t in clientList:
            t.start()
        sleep_bar(properties["exp_sec"], "Waiting for client to finish")
        # Wait for all clients to finish
        for t in clientList:
            t.join(9600)

        collectData(propFile, localExpDir, remotePath, nbClients=nbClients, nbSuborams=nbSuborams, nbBalancers=nbBalancers)

        # Kill processes
        hosts = ["%s@%s" % (username, ip) for ip in balancerIpList]
        executeParallelBlockingRemoteCommand(hosts, "pkill -f load_balancer", sshKey, printCmd=False)
        allSuboramIps = []
        for suboramIps in suboramIpList:
            allSuboramIps.extend(suboramIps)
        hosts = ["%s@%s" % (username, ip) for ip in allSuboramIps]
        executeParallelBlockingRemoteCommand(hosts, "pkill -f suboram", sshKey, printCmd=False)

        return calculateParallel(propFile, localExpDir, roundName, nbBatchSz, nbClients)

def runVaryDataSzExperiment(propFile, nbClients, nbSuborams, nbBalancers, nbBatchSz, nbEpochMs, it):
    properties = loadPropertyFile(propFile)
    minDataSz = properties["min_datasize"]
    maxDataSz = properties["max_datasize"]
    maxLatency = properties['max_latency_ms']
    currLatency = 0
    GRANULARITY = 100000
    lDataSz = minDataSz / GRANULARITY
    rDataSz = maxDataSz / GRANULARITY
    while lDataSz <= rDataSz:
        mDataSz = int(math.floor((lDataSz + rDataSz)/2))
        if mDataSz == 0:
            break
        nbDataSz = mDataSz*GRANULARITY
        results = runSingleExperiment(propFile, nbClients, nbSuborams, nbBalancers, nbDataSz, nbBatchSz, nbEpochMs, it)
        currLatency = float(results[1].strip().split(" ")[0])
        if currLatency < maxLatency:
            lDataSz = mDataSz+1
        elif currLatency > maxLatency:
            rDataSz = mDataSz-1
        else:
            break

def runVaryBatchSzExperiment(propFile, nbSuborams, nbBalancers):
    properties = loadPropertyFile(propFile)
    nbRepetitions = properties['nbrepetitions']
    minBatchSz = properties["min_batchsz"]
    maxBatchSz = properties["max_batchsz"]
    batchSzGranularity = properties["batchsz_granularity"]
    max_throughput = 0
    maxLatency = properties["max_latency_ms"]
    min_latency = float('inf')
    for nbClients in properties['nbclients']:
        dataSizes = properties['nbdatasize']
        if 'data_for_suboram' in properties:
            dataSizes = properties["data_for_suboram"][str(nbSuborams)]
        for nbDataSz in dataSizes:
            currLatency = 0
            lBatchSz = minBatchSz / batchSzGranularity 
            rBatchSz = maxBatchSz / batchSzGranularity 
            while lBatchSz <= rBatchSz:
                mBatchSz = int(math.floor((lBatchSz + rBatchSz)/2))
                nbBatchSz = mBatchSz*batchSzGranularity
                for nbEpochMs in properties['nbepochms']:
                    for it in range(0, nbRepetitions):
                        results = runSingleExperiment(propFile, nbClients, nbSuborams, nbBalancers, nbDataSz, nbBatchSz, nbEpochMs, it)
                        currLatency = float(results[1].strip().split(" ")[0])
                        throughput = float(results[1].strip().split(" ")[-1])
                        if throughput > max_throughput and currLatency <= maxLatency :
                            max_throughput = throughput
                        if currLatency < min_latency:
                            min_latency = currLatency

                if currLatency < maxLatency:
                    lBatchSz = mBatchSz+1
                elif currLatency > maxLatency:
                    rBatchSz = mBatchSz-1
                else:
                    tqdm.write("best batch size found for %d suborams, %d load balancers: %d" % (nbSuborams, nbBalancers, mBatchSz))
                    break
    return max_throughput, min_latency

def runAdaptiveExperiment(propFile):
    properties = loadPropertyFile(propFile)
    max_balancers = max(properties['nbbalancers'])
    max_suborams = max(properties['nbsuborams'])
    experiment_bar = tqdm(total=max_balancers+max_suborams-3, position=0, desc="[Experiment progress]")
    runVaryBatchSzExperiment(propFile, 3, 1)
    experiment_bar.update(1)
    curr_suborams = 3
    curr_balancers = 1
    while curr_balancers <= max_balancers and curr_suborams <= max_suborams:
        # Try increment both
        xput_inc_balancer = 0
        lat_inc_balancer = float('inf')
        xput_inc_suboram = 0
        lat_inc_suboram = float('inf')
        if (curr_balancers + 1 <= max_balancers):
            xput_inc_balancer, lat_inc_balancer = runVaryBatchSzExperiment(propFile, curr_suborams, curr_balancers+1)
        if (curr_suborams + 1 <= max_suborams):
            xput_inc_suboram, lat_inc_suboram = runVaryBatchSzExperiment(propFile, curr_suborams+1, curr_balancers)
        #print "\n\n\n\n\n"
        #print "[%d balancers and %d suborams] throughput: %d, latency: %d" % (curr_balancers+1, curr_suborams, xput_inc_balancer, lat_inc_balancer)
        #print "[%d balancers and %d suborams] throughput: %d, latency: %d" % (curr_balancers, curr_suborams+1, xput_inc_suboram, lat_inc_suboram)
        if xput_inc_balancer > xput_inc_suboram:
            curr_balancers += 1
            tqdm.write("Incrementing balancers")
        elif xput_inc_suboram > xput_inc_balancer:
            curr_suborams += 1
            tqdm.write("Incrementing suborams")
        else:
            if lat_inc_balancer < lat_inc_suboram:
                curr_balancers += 1
                tqdm.write("Incrementing balancers")
            else:
                curr_suborams += 1
                tqdm.write("Incrementing suborams")
        experiment_bar.update(1)
    experiment_bar.close()

def runExperiment(propFile):
    properties = loadPropertyFile(propFile)
    if (properties["experiment_type"] == "adaptive"):
        return runAdaptiveExperiment(propFile)
    elif (properties["experiment_type"] == "bench_make_batch"):
        return balancer_benchmark.runExperiment(propFile)
    elif (properties["experiment_type"] == "bench_match_responses"):
        return balancer_benchmark.runExperiment(propFile)
    elif (properties["experiment_type"] == "bench_sort"):
        return suboram_benchmark.runExperiment(propFile)
    elif (properties["experiment_type"] == "bench_process_batch"):
        return suboram_benchmark.runExperiment(propFile)
    elif (properties["experiment_type"] == "bench_process_batch_threads"):
        return suboram_benchmark.runExperiment(propFile)

    # Name of the experiment that will be run
    sshKey = properties["ssh_key_file"]
    username = properties["username"]

    machines = properties["machines"]
    balancerIpList = machines["balancer_ips"]
    suboramIpList = machines["suboram_ips"]

    # number of times each experiment will be run
    nbRepetitions = properties['nbrepetitions']

    experiment_type = properties["experiment_type"]

    # Kill processes
    hosts = ["%s@%s" % (username, ip) for ip in balancerIpList]
    executeParallelBlockingRemoteCommand(hosts, "pkill -f load_balancer", sshKey, printCmd=False)
    allSuboramIps = []
    for suboramIps in suboramIpList:
        allSuboramIps.extend(suboramIps)
    hosts = ["%s@%s" % (username, ip) for ip in allSuboramIps]
    executeParallelBlockingRemoteCommand(hosts, "pkill -f suboram", sshKey, printCmd=False)

    total_experiments = 0
    for nbClients in properties['nbclients']:
        for nbSuborams in properties['nbsuborams']:
            for nbBalancers in properties['nbbalancers']:
                dataSizes = properties['nbdatasize']
                if 'data_for_suboram' in properties:
                    dataSizes = properties["data_for_suboram"][str(nbSuborams)]
                if experiment_type == "vary_data_size":
                    dataSizes = [None]
                for nbDataSz in dataSizes:
                    for nbBatchSz in properties['nbbatchsz']:
                        for nbEpochMs in properties['nbepochms']:
                            for it in range(0, nbRepetitions):
                                total_experiments += 1
    experiment_bar = tqdm(total=total_experiments, position=0, desc="[Experiment progress]")

    # Run for each round, nbRepetitions time.
    for nbClients in properties['nbclients']:
        for nbSuborams in properties['nbsuborams']:
            for nbBalancers in properties['nbbalancers']:
                dataSizes = properties['nbdatasize']
                if 'data_for_suboram' in properties:
                    dataSizes = properties["data_for_suboram"][str(nbSuborams)]
                if experiment_type == "vary_data_size":
                    dataSizes = [None]
                for nbDataSz in dataSizes:
                    for nbBatchSz in properties['nbbatchsz']:
                        for nbEpochMs in properties['nbepochms']:
                            for it in range(0, nbRepetitions):
                                if experiment_type == "vary_data_size":
                                    runVaryDataSzExperiment(propFile, nbClients, nbSuborams, nbBalancers, nbBatchSz, nbEpochMs, it)
                                else:
                                    runSingleExperiment(propFile, nbClients, nbSuborams, nbBalancers, nbDataSz, nbBatchSz, nbEpochMs, it)
                                experiment_bar.update(1)
    experiment_bar.close()
    return "Done with experiment"

def collectData(propFile, localFolder, remoteFolder, nbClients=-1, nbSuborams=-1, nbBalancers=-1):
    tqdm.write(bordered("Collecting data"))
    properties = loadPropertyFile(propFile)
    sshKey = properties["ssh_key_file"]
    username = properties["username"]
    machines = properties["machines"]
    clientIpList = machines["client_ips"][:nbClients]
    suboramIpList = machines["suboram_ips"][:nbSuborams]
    balancerIpList = machines["balancer_ips"][:nbBalancers]

    threads = []
    threads.extend(getDirectory(localFolder, username, clientIpList, remoteFolder, sshKey, printfn=tqdm.write))
    threads.extend(getDirectory(localFolder, username, balancerIpList, remoteFolder, sshKey, printfn=tqdm.write))
    for suboramIps in suboramIpList:
        threads.extend(getDirectory(localFolder, username, suboramIps, remoteFolder, sshKey, printfn=tqdm.write))
    for t in tqdm(threads, desc="[scp'ing results back]"):
        t.join()

# Computes experiment results and outputs all results in results.dat
# For each round in an experiment run the "generateData" method as a separate
# thread
def calculateParallel(propertyFile, localExpDir, roundName, batchSz, numClients):
    tqdm.write(bordered("Calculating results"))
    properties = loadPropertyFile(propertyFile)
    if not properties:
        tqdm.write("Empty property file, failing")
        return
    machines = properties["machines"]
    experimentName = properties['experiment_name']
    if (not localExpDir):
            localProjectDir = properties['local_project_dir']
            expDir = properties['experiment_dir']
            localExpDir = localProjectDir / expDir
    processedResultsPath = localExpDir / roundName / "processed_results.dat"
    tqdm.write(f"Writing results to {processedResultsPath}")
    fileHandler = open(processedResultsPath, "w+")
    time = int(properties['exp_sec'])
    results = dict()
    #try:
    file_list = []
    for i in range(numClients):
        client_ip = machines["client_ips"][i]
        file_list.append(localExpDir / roundName / f"results_{client_ip}.dat")
    
    resultsPath = localExpDir / roundName / "results.dat"
    combineFiles(file_list, resultsPath)
    tqdm.write("Reading from file: " + str(resultsPath))
    generateData(results, resultsPath, 1, time, batchSz)
    #except:
    #    print "No results file found"

    tqdm.write("Finished Processing Batch")
           #executingThreads = list()
    sortedKeys = sorted(results.keys())
    for key in sortedKeys:
        fileHandler.write(results[key])
    fileHandler.flush()
    fileHandler.close()
    tqdm.write("Finished collecting data")
    return results

# Generates data using the math functions available in math_util
# Expects latency to be in the third column of the output file
def generateData(results,folderName, clients, time, batchSz):
    tqdm.write("Generating Data for " + str(folderName))
    result = str(computeMean(folderName,1)) + " "
    result+= str(computeMin(folderName,1)) + " "
    result+= str(computeMax(folderName,1)) + " "
    result+= str(computeVar(folderName,1)) + " "
    result+= str(computeStd(folderName,1)) + " "
    result+= str(computePercentile(folderName,1,50)) + " "
    result+= str(computePercentile(folderName,1,75)) + " "
    result+= str(computePercentile(folderName,1,90)) + " "
    result+= str(computePercentile(folderName,1,95)) + " "
    result+= str(computePercentile(folderName,1,99)) + " "
    tqdm.write("Generated data for up through 99 percentile")
    result+= str(computeThroughput(folderName,1,time, batchSz)) + " \n"
    results[clients]=result

def summarizeData(propertyFile):
    properties = loadPropertyFile(propertyFile)
    if not properties:
        print("Empty property file, failing")
        return
    if "no_summarize" in properties:
        return
    expDir = properties['experiment_dir']
    localProjectDir = Path(properties['local_project_dir'])
    localExpDir = localProjectDir / expDir
    outFile = localExpDir / "final_results.dat"
    fFinal = open(outFile, "w")

    def sort_key(x):
        nbClients, nbDataSz, nbSuborams, nbBalancers, nbBatchSz, nbEpochMs, it = [int(i) for i in x.name.split("_")]
        return f"{nbClients}_{nbEpochMs}_{nbSuborams+nbBalancers:03d}_{nbSuborams:03d}_{nbBalancers:03d}_{nbDataSz:09d}_{nbBatchSz:09d}"

    localRoundFolders = sorted([x for x in localExpDir.iterdir() if x.is_dir()], key=sort_key)
    for localRoundFolder in localRoundFolders:
        nbClients, nbDataSz, nbSuborams, nbBalancers, nbBatchSz, nbEpochMs, it = localRoundFolder.name.split("_")
        roundResults = localRoundFolder / "processed_results.dat"
        fRound = open(roundResults, "r")
        fFinal.write(str(nbClients) + " " + str(nbDataSz) + " " + str(nbSuborams) + " " + str(nbBalancers) + " " + str(nbEpochMs) + " " + str(it) + " " + fRound.readline())
        fRound.close()
    fFinal.close()

def graphData(propertyFile):
    properties = loadPropertyFile(propertyFile)
    localProjectDir = Path(properties['local_project_dir'])
    expDir = properties['experiment_dir']
    localExpDir = localProjectDir / expDir
    outFile = localExpDir / "final_results.dat"
    figure = properties["figure"]
    print(bordered(f"Plotting Figure {figure}"))
    cmpFigDir = localProjectDir / 'artifact_figures'
    artifactFigDir = localProjectDir / 'artifact_figures' / 'artifact'
    paperFigDir = localProjectDir / 'artifact_figures' / 'paper'

    if figure == "9":
        figName = '9.pdf'
        title = "Figure 9"
        script = Path("fig/scale_throughput.py")
        paperDataFile = paperFigDir / "scale_throughput.dat"
    elif figure == "10a":
        figName = '10a.pdf'
        title = "Figure 10a"
        script = Path("fig/scale_data.py")
        paperDataFile = paperFigDir / "scale_data.dat"
    elif figure == "10b":
        figName = '10b.pdf'
        title = "Figure 10b"
        script = Path("fig/suborams_vs_latency.py")
        paperDataFile = paperFigDir / "suborams_vs_latency.dat"
    elif figure == "12a":
        figName = '12a.pdf'
        title = "Figure 12a"
        script = Path("fig/sort_parallel.py")
        paperDataFile = paperFigDir / "sort_parallel.dat"
    elif figure == "12b":
        figName = '12b.pdf'
        title = "Figure 12b"
        script = Path("fig/suboram_parallel.py")
        paperDataFile = paperFigDir / "suboram_parallel.dat"
    elif figure == "13":
        figName = '13.pdf'
        title = "Figure 13"
        script = Path("fig/scooby_oblix.py")
        paperDataFile = paperFigDir / "scooby_oblix.dat"
    else:
        print("Unsupported figure:", figure)
        return

    cmd = f'python3 {script} -b {artifactFigDir / "baselines.json"} -l -t "Artifact Evaluation" {outFile} {localExpDir / figName}'
    executeCommand(cmd)
    cmd = f'python3 {script} -b {paperFigDir / "baselines.json"} -l -t "Paper {title}" {paperDataFile} {paperFigDir / figName}'
    executeCommand(cmd)
    cmd = f"cp {localExpDir / figName } {artifactFigDir}"
    executeCommand(cmd)
    cmd = f"pdfjam --landscape --nup 2x1 {artifactFigDir / figName} {paperFigDir / figName} --outfile {cmpFigDir / figName}"
    executeCommand(cmd)


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
    parser.add_argument('-g', '--graph', action='store_true',
                        help='graph experiment results (default: false)')
    parser.add_argument('-c', '--cleanup', action='store_true',
                        help='cleanup instances(default: false)')
    parser.add_argument('-f', '--figure', help='figure to reproduce')
    parser.add_argument('--config', help='config file for experiment')
    args = parser.parse_args()

    if not args.cleanup and not args.figure and not args.config:
        parser.print_help()
        parser.exit()

    if not args.provision and not args.setup and not args.run and not args.graph and not args.cleanup and not args.summarize:
        parser.print_help()
        parser.exit()


    if args.figure:
        config_file = figure_to_config_file(args.figure)
    else:
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
        #graphData(config_file)
    if args.summarize:
        summarizeData(config_file)
    if args.graph:
        graphData(config_file)
    if args.cleanup:
        print("Cleaning up... this will take a few minutes.")
        cleanupExperiment()


if __name__ == '__main__':
    main()

# TODO fix what's going on with load balancer executable

#cleanupExperiment()
#provisionExperiment(PROPERTY_FILE)
#setupExperiment(PROPERTY_FILE)
#runExperiment(PROPERTY_FILE)
#summarizeData(PROPERTY_FILE)
#cleanupExperiment()
