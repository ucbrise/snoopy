# pylint: disable=C0103, C0111

import os
import os.path
import sys
import argparse
import datetime
import time
import random
import multiprocessing
import subprocess
import queue

from pathlib import Path, PurePath

sys.path.append("..")

from util_py3.ssh_util import *
from util_py3.azure_util import *
from util_py3.prop_util import *
from util_py3.math_util import *
from util_py3.graph_util import *

proxyKeyword = "proxy"
storageKeyword = "storage"
clientKeyword = "client"
nbRepetitions = 1

PROP_FILE = "config/batch-500.json"

az = AzureSetup("obladi")

def loadOptionalKey(properties, key):
    try:
        prop = properties[key]
        return prop
    except KeyError:
        return null


def provisionExperiment(propFile):
    properties = loadPropertyFile(propFile)
    compute_client, network_client = az.runAzureSetup(properties["location"])
    #cleanupAzure()
    client_result_queue = [queue.Queue() for _ in properties["instance_types"]["client"]]
    proxy_result_queue = queue.Queue()
    storage_result_queue = [queue.Queue() for _ in range(properties["num_instances"]["storage"])]
    threads = []
    total_machines = len(client_result_queue)*properties["num_instances"]["client"] + properties["num_instances"]["proxy"] + properties["num_instances"]["storage"]*properties["num_instances"]["storage"]
    pbar = tqdm(total=total_machines*3, desc="[Provisioning resources for VMs...]")
    for i, client_instance_type in enumerate(properties["instance_types"]["client"]):
        threads.extend(
            az.startAzureInstancesAsync(
                compute_client, network_client, "client",
                client_instance_type, properties["location"],
                properties["image_path"], properties["ssh_key_pub"],
                properties["num_instances"]["client"],
                client_result_queue[i],
                pbar,
            )
        )
    threads.extend(
        az.startAzureInstancesAsync(
            compute_client, network_client, "proxy",
            properties["instance_types"]["proxy"], properties["location"],
            properties["image_path"], properties["ssh_key_pub"],
            properties["num_instances"]["proxy"],
            proxy_result_queue,
            pbar,
        )
    )
    for i in range(properties["num_instances"]["storage"]):
        threads.extend(
            az.startAzureInstancesAsync(
                compute_client,
                network_client, ("storage%d-") % (i),
                properties["instance_types"]["storage"], properties["location"],
                properties["image_path"], properties["ssh_key_pub"],
                properties["num_instances"]["storage"],
                storage_result_queue[i],
                pbar,
            )
        )

    for t in threads:
        t.join()
    pbar.close()

    client_vms = []
    client_ips = []
    client_private_ips = []
    for i, client_instance_type in enumerate(properties["instance_types"]["client"]):
        vms = []
        ips = []
        private_ips = []
        for _ in range(properties["num_instances"]["client"]):
            vm_poller, ip, private_ip = client_result_queue[i].get()
            vms.append(vm_poller)
            ips.append(ip)
            private_ips.append(private_ip)
        client_vms = client_vms + vms
        client_ips.append(ips)
        client_private_ips.append(private_ips)

    proxy_vms = []
    proxy_ips = []
    proxy_private_ips = []
    for _ in range(properties["num_instances"]["proxy"]):
        vm_poller, ip, private_ip = proxy_result_queue.get()
        proxy_vms.append(vm_poller)
        proxy_ips.append(ip)
        proxy_private_ips.append(private_ip)
    
    storage_vms = []
    storage_ips = []
    storage_private_ips = []
    for i in range(properties["num_instances"]["storage"]):
        vms = []
        ips = []
        private_ips = []
        for _ in range(properties["num_instances"]["storage"]):
            vm, ip, private_ip = storage_result_queue[i].get()
            vms.append(vm)
            ips.append(ip)
            private_ips.append(private_ip)
        storage_vms = storage_vms + vms
        storage_ips.append(ips)
        storage_private_ips.append(private_ips)


    properties["client_ips"] = client_ips
    properties["client_private_ips"] = client_private_ips
    properties["proxy_ips"] = proxy_ips
    properties["proxy_private_ips"] = proxy_private_ips
    properties["storage_ips"] = storage_ips
    properties["storage_private_ips"] = storage_private_ips
    properties['proxy_ip_address'] = proxy_private_ips[0]
    properties['remote_store_ip_address'] = storage_private_ips[0][0]   # TODO: add list
    properties['clients'] = client_ips
    writePropFile(propFile, properties)
    return client_vms, proxy_vms, storage_vms


def cleanupExperiment():
    az.cleanupAzure()


class Config(object):
    def __init__(self, properties):
        # Username for ssh-ing.
        self.user = properties['username']
        # Name of the experiment that will be run
        self.experimentName = properties['experiment_name']
        # Project dir on the local machine
        self.localProjectDir = Path(properties['local_project_dir'])
        # Project dir on the remote machine
        self.remoteProjectDir = PurePath(properties['remote_project_dir'])
        # Source directory on the local machine (for compilation)
        self.localSrcDir = Path(properties['local_src_dir'])

        # this experiment had three components, this determines whether they get started
        self.useProxy = toBool(properties['useproxy'])
        self.useStorage = toBool(properties['usestorage'])
        self.useLoader = toBool(properties['useloader'])

        #### LOADING/GENERATING JAR FILES ####
        self.jarName = properties['jar']
        self.clientMainClass = properties['clientmain']
        self.proxyMainClass = loadOptionalKey(properties, 'proxymain')
        self.storageMainClass = loadOptionalKey(properties, 'storagemain')
        self.loaderMainClass = loadOptionalKey(properties, 'loadermain')
        self.javaCommandClient = properties['javacommandclient']
        self.javaCommandServer = properties['javacommandserver']

        self.experimentDir = loadOptionalKey(properties, 'experiment_dir')

        self.sshKeyFile = properties["ssh_key_file"]
        self.clientIpsByInstanceType = properties["client_ips"]
        self.clientPrivateIpsByInstanceType = properties["client_private_ips"]
        self.clientIps = []
        self.clientPrivateIps = []
        for ips, private_ips in zip(self.clientIpsByInstanceType, self.clientPrivateIpsByInstanceType):
            self.clientIps.extend(ips)
            self.clientPrivateIps.extend(private_ips)
        self.proxyIps = properties["proxy_ips"]
        self.proxyIp = self.proxyIps[0]
        self.proxyPrivateIps = properties["proxy_private_ips"]
        self.proxyPrivateIp = self.proxyPrivateIps[0]
        self.storageIpsByCluster = properties["storage_ips"]
        self.storagePrivateIpsByCluster = properties["storage_private_ips"]
        self.storageIps = []
        self.storagePrivateIps = []
        for ips, private_ips in zip(self.storageIpsByCluster, self.storagePrivateIpsByCluster):
            self.storageIps.extend(ips)
            self.storagePrivateIps.extend(private_ips)

        self.nbRounds = len(properties['nbclients'])
        self.nbClientTypes = len(properties["instance_types"]["client"])
        self.logFolders = properties['log_folder']
        self.currClientIps = []
        self.currClientPrivateIps = []


def setupExperiment(propertyFile):
    properties = loadPropertyFile(propertyFile)
    cfg = Config(properties)

    # Maven-specific code
    mavenDir = cfg.localSrcDir / "target"

    # The experiment folder is generated with the following path:
    # results/experimentName/Date
    # The date is used to distinguish multiple runs of the same experiment
    expFolder = PurePath('results', cfg.experimentName)
    expDir = expFolder / datetime.datetime.now().strftime("%Y:%m:%d:%H:%M")
    properties['experiment_dir'] = str(expDir)

    # LocalPath and RemotePath describe where data will be stored on the remote machine
    # and on the local machine
    localPath = cfg.localProjectDir / expDir
    remotePath = cfg.remoteProjectDir / expDir

    ##### UPDATE DB DIRECTORY #####
    properties['db_file_path'] = str(cfg.remoteProjectDir / expFolder / properties['db_file_name'])

    #### LOADING/GENERATING JAR FILES ####
    print("Using Proxy " + str(cfg.useProxy))
    print("Using Storage " + str(cfg.useStorage))
    print("Using Loader " + str(cfg.useLoader))

    # Compile Jars
    print("Setup: Compiling Jars")
    executeCommand(f"cd {cfg.localSrcDir}; mvn install")
    executeCommand(f"cd {cfg.localSrcDir}; mvn package")

    fileExists = (mavenDir / cfg.jarName).is_file()
    if not fileExists:
        print("Error: Incorrect Jar Name")
        exit()

    #### GENERATING EXP DIRECTORY ON ALL MACHINES ####
    print("Creating Experiment directory")
    for c in cfg.clientIps:
        print(c)
        mkdirRemote(cfg.user, c, remotePath, cfg.sshKeyFile)
    if cfg.useProxy:
        for p in cfg.proxyIps:
            print(p)
            mkdirRemote(cfg.user, p, remotePath, cfg.sshKeyFile)
    if cfg.useStorage:
        print("Reached here")
        for s in cfg.storageIps:
            mkdirRemote(cfg.user, s, remotePath, cfg.sshKeyFile)
    executeCommand(f"mkdir -p {localPath}")
    tmpSymlink = "tmp_symlink"
    symlinkDir = os.path.join(cfg.localProjectDir, "results", cfg.experimentName, "latest")
    os.symlink(localPath, tmpSymlink)
    os.rename(tmpSymlink, symlinkDir)

    #### SENDING JARS TO ALL MACHINES ####

    # Send Jars
    print("Sending Jars to all Machines")
    j = mavenDir / cfg.jarName
    sendFileHosts(j, cfg.user, cfg.clientIps, remotePath, cfg.sshKeyFile)
    if cfg.useProxy:
        sendFileHosts(j, cfg.user, cfg.proxyIps, remotePath, cfg.sshKeyFile)
    if cfg.useStorage:
        sendFileHosts(j, cfg.user, cfg.storageIps, remotePath, cfg.sshKeyFile)
    executeCommand(f"cp {j} {localPath}")

    # Create file with git hash
    executeCommand(f"cp {propertyFile} {localPath}")
    gitHash = getGitHash(cfg.localSrcDir)
    print(f"Saving Git Hash {gitHash}")
    gitHashFile = localPath / "git.txt"
    executeCommand(f"touch {gitHashFile}")
    with open(gitHashFile, 'ab') as f:
        f.write(gitHash)

    ## Write back the updated property file to the json
    writePropFile(propertyFile, properties)
    executeCommand(f"cp {propertyFile} {localPath}")
    return localPath


# Runs the actual experiment
def runExperiment(propertyFile, deleteTable = False): 
    properties = loadPropertyFile(propertyFile)
    cfg = Config(properties)
    try:
        javaCommandStorage = properties['javacommandstorage']
    except KeyError:
        javaCommandStorage = cfg.javaCommandServer

    try:
        noKillStorage = properties['no_kill_storage']
    except KeyError:
        noKillStorage = False

    try:
        simulateLatency = int(properties['simulate_latency'])
    except KeyError:
        simulateLatency = 0

    remoteExpDir = cfg.remoteProjectDir / cfg.experimentDir
    localExpDir = cfg.localProjectDir / cfg.experimentDir
    #TODO(natacha): cleanup
    try:
        reuseData = toBool(properties['reuse_data'])
        print("Reusing Data " + str(reuseData))
    except KeyError:
        reuseData = False

    #properties = updateDynamoTables(properties, experimentName)

    # Setup latency on appropriate hosts if
    # simulated
    print("WARNING: THIS IS HACKY AND WILL NOT WORK WHEN CONFIGURING MYSQL")
    if simulateLatency:
        print("Simulating a " + str(simulateLatency) + " ms")
        if cfg.useProxy:
            setupTC(cfg.proxyIp, simulateLatency, cfg.storagePrivateIps, cfg.sshKeyFile)
            if cfg.useStorage:
                for c in cfg.storageIps:
                    setupTC(c, simulateLatency, cfg.proxyPrivateIps, cfg.sshKeyFile)
        else:
            # Hacky if condition for our oram tests without proxy
            # Because now latency has to be between multiple hostsu
            if cfg.useStorage:
                for c in cfg.clientIps:
                    setupTC(c, simulateLatency, cfg.storagePrivateIps, cfg.sshKeyFile)
                for s in cfg.storageIps:
                    setupTC(s, simulateLatency, cfg.clientPrivateIps, cfg.sshKeyFile)
            else:
                raise Exception("TODO MYSQL")
    first = True
    for client_idx in range(0, cfg.nbClientTypes):
        dataLoaded = False
        for i in range(0, cfg.nbRounds):
            time.sleep(10)
            for it in range(0, nbRepetitions):
                try:
                    print("Running Round: " + str(i) + " Iter " + str(it))
                    nbClients = int(properties['nbclients'][i])
                    print("Number of clients " + str(nbClients))
                    clientInstance = properties["instance_types"]["client"][client_idx]
                    cfg.currClientIps = cfg.clientIpsByInstanceType[client_idx]
                    cfg.currClientPrivateIps = cfg.clientPrivateIpsByInstanceType[client_idx]
                    localRoundFolder = localExpDir / f"{nbClients}_{clientInstance}_{it}"
                    remoteRoundFolder = remoteExpDir / f"{nbClients}_{clientInstance}_{it}"
                    print("Round Folder:", localRoundFolder)
                    localPath = localRoundFolder
                    remotePath = remoteRoundFolder
                    print("Remote Path:", remotePath)
                    executeCommand(f"mkdir -p {localPath}")
                    logFolder = remotePath / cfg.logFolders
                    properties['log_folder'] = str(logFolder)
                    localProp = localPath / "properties"
                    remoteProp = remotePath / "properties"
                    properties['exp_dir'] = str(remotePath)

                    # Create folders on appropriate hosts
                    mkdirRemoteHosts(cfg.user, cfg.currClientIps, remotePath, cfg.sshKeyFile)
                    mkdirRemoteHosts(cfg.user, cfg.currClientIps, logFolder, cfg.sshKeyFile)
                    if cfg.useProxy:
                        mkdirRemoteHosts(cfg.user, cfg.proxyIps, remotePath, cfg.sshKeyFile)
                        mkdirRemoteHosts(cfg.user, cfg.proxyIps, logFolder, cfg.sshKeyFile)
                    if cfg.useStorage:
                        mkdirRemoteHosts(cfg.user, cfg.storageIps, remotePath, cfg.sshKeyFile)
                        mkdirRemoteHosts(cfg.user, cfg.storageIps, logFolder, cfg.sshKeyFile)

                    properties['proxy_listening_port'] = str(random.randint(20000, 30000))
                    #properties['proxy_listening_port'] = str(12346)

                    if (first or (not noKillStorage)):
                        properties['remote_store_listening_port'] = str(random.randint(30000,40000))
                        #properties['remote_store_listening_port'] = str(12345)

                    localProp = localPath / "properties"
                    remoteProp = remotePath / "properties"

                    # start storage
                    print("Start Storage (Having Storage " + str(cfg.useStorage) + ")")
                    if (cfg.useStorage and (first or (not noKillStorage))):
                        first = False
                        for (idx, s) in enumerate(cfg.storageIps):
                            print("Starting Storage again")
                            sid = nbClients + 2 + idx
                            properties['node_uid'] = str(sid)
                            properties['node_ip_address'] = cfg.storagePrivateIps[idx]
                            properties['node_listening_port'] = properties['remote_store_listening_port']
                            localProp_ = localProp.parent / (localProp.name + "_storage%d.json" % sid)
                            remoteProp_ = remoteProp.parent / (remoteProp.name + "_storage%d.json" % sid)
                            writePropFile(localProp_, properties)
                            print("Sending Property File and Starting Server")
                            sendFileHosts(localProp_, cfg.user, cfg.storageIps, remotePath, cfg.sshKeyFile)
                            storageLog = remotePath / f"storage{sid}.log"
                            storageErrLog = remotePath / f"storage_err{sid}.log"
                            cmd = f"cd {remoteExpDir}; {javaCommandStorage} -cp {cfg.jarName} {cfg.storageMainClass} {remoteProp_} 1> {storageLog} 2> {storageErrLog}"
                            t = executeNonBlockingRemoteCommand("%s@%s" % (cfg.user, s), cmd, cfg.sshKeyFile)
                            t.start()
                    else:
                        print("Storage already started")

                    time.sleep(30)
                    # start proxy
                    print("Start Proxy (Having Proxy " + str(cfg.useProxy) + ")")
                    if cfg.useProxy:
                        sid = nbClients + 1
                        properties['node_uid'] = str(sid)
                        properties['node_ip_address'] = cfg.proxyPrivateIp
                        properties['node_listening_port'] = properties['proxy_listening_port']
                        localProp_ = localProp.parent / (localProp.name + "_proxy.json")
                        remoteProp_ = remoteProp.parent / (remoteProp.name+ "_proxy.json")
                        writePropFile(localProp_, properties)
                        print("Sending Property File and Starting Server")
                        sendFile(localProp_, cfg.proxyIp, cfg.user, remotePath, cfg.sshKeyFile)
                        proxyLog = remotePath / f"proxy{sid}.log"
                        proxyErrLog = remotePath / f"proxy_err{sid}.log"
                        cmd = f"cf {remoteExpDir}; {cfg.javaCommandServer} -cp {cfg.jarName} {cfg.proxyMainClass} {remoteProp_} 1> {proxyLog} 2> {proxyErrLog}"
                        print(cmd)
                        t = executeNonBlockingRemoteCommand("%s@%s" % (cfg.user, cfg.proxyIp), cmd, cfg.sshKeyFile)
                        t.start()
                        time.sleep(30)

                    oldDataSet = None
                    ## Load Data ##
                    print("Start Loader (Having Loader " + str(cfg.useLoader) + ")")
                    if cfg.useLoader and ((not dataLoaded) or (not reuseData)):
                        dataLoaded = True
                        localProp_ = localProp.parent / (localProp.name + "_loader.json")
                        remoteProp_ = remoteProp.parent / (remoteProp.name + "_loader.json")
                        ip = cfg.currClientIps[0]
                        properties['node_uid'] = str(nbClients + 2 + len(cfg.storageIps))
                        properties['node_ip_address'] = cfg.clientPrivateIps[0]
                        properties.pop('node_listening_port', None)

                        oldDataSet = properties['key_file_name']
                        dataset_remloc = remotePath+ "/" + properties['key_file_name']
                        dataset_localoc = localPath+ "/" + properties['key_file_name']
                        properties['key_file_name'] = dataset_remloc
                        writePropFile(localProp, properties)
                        sendFile(localProp_, cfg.currClientIps[0], cfg.user, remotePath, cfg.sshKeyFile)
                        cmd = f"cd {remoteExpDir}; {cfg.javaCommandServer} -cp {cfg.jarName} {cfg.loaderMainClass} {remoteProp_} 1> {remotePath / 'loader.log'} 2> {remotePath / 'loader_err.log'}"
                        # Generate data set via executing the loader
                        executeRemoteCommand("%s@%s" %(cfg.user, cfg.currClientIps[0]), cmd, cfg.sshKeyFile)
                        getFile(dataset_remloc, cfg.currClientIps[:1], dataset_localoc, cfg.sshKeyFile)
                        # Once dataset has been executed, send it out to all clients
                        sendFileHosts(dataset_localoc, cfg.user, cfg.currClientIps, dataset_remloc, cfg.sshKeyFile);

                    ## Start clients ##
                    nbMachines = len(cfg.currClientIps)
                    client_list = list()
                    for cid in range(nbClients, 0, -1):
                        ip = cfg.currClientIps[cid % nbMachines]
                        private_ip = cfg.currClientPrivateIps[cid % nbMachines]
                        properties['node_uid'] = str(cid)
                        properties['node_ip_address'] = private_ip
                        properties.pop('node_listening_port', None)
                        localProp_ = localProp.parent / (localProp.name + "client" + str(cid) + ".json")
                        oldRunName = properties['run_name']
                        remoteProp_ = remoteProp.parent / (remoteProp.name + "client" + str(cid) + ".json")
                        properties['run_name'] = str(remotePath / (str(cid) + "_" + properties['run_name']))
                        with open(localProp_, 'w') as fp:
                            json.dump(properties, fp, indent = 2, sort_keys=True)
                        sendFile(localProp_, ip, cfg.user, remoteProp_, cfg.sshKeyFile)
                        clientLog = remotePath / f"client_{ip}_{cid}.log"
                        clientErrLog = remotePath / f"client_{ip}_{cid}_err.log"
                        cmd = f"cd {remoteExpDir}; {cfg.javaCommandClient} -cp {cfg.jarName} {cfg.clientMainClass} {remoteProp_} 1> {clientLog} 2> {clientErrLog}"
                        # uncomment for debugging
                        #cmd = f"cd {remoteExpDir}; ({cfg.javaCommandClient} -cp {cfg.jarName} {cfg.clientMainClass} {remoteProp_} | tee {clientLog}) 3>&1 1>&2 2>&3 | tee {clientErrLog}"
                        
                        t = executeNonBlockingRemoteCommand("%s@%s" % (cfg.user, ip), cmd, cfg.sshKeyFile)
                        client_list.append(t)
                        properties['run_name'] = oldRunName

                    print("Start clients")
                    for t in client_list:
                        t.start()
                    for t in client_list:
                        t.join(9600)
                    collectData(propertyFile, localPath, remotePath, cfg.currClientIps)
                    time.sleep(60)
                    print("Finished Round")
                    print("---------------")
                    if oldDataSet is not None:
                        properties['key_file_name'] = oldDataSet

                    for c in cfg.currClientIps:
                        try:
                            executeRemoteCommandNoCheck("%s@%s" % (cfg.user, c), "ps -ef | grep java | grep -v grep | grep -v bash | awk '{print \$2}' | xargs -r kill -9", cfg.sshKeyFile)
                        except Exception as e:
                            print(" ")
                    if cfg.useProxy:
                        try:
                            print("Killing Proxy" + str(cfg.proxyIp))
                            executeRemoteCommandNoCheck("%s@%s" % (cfg.user, cfg.proxyIp), "ps -ef | grep java | grep -v grep | grep -v bash | awk '{print \$2}' | xargs -r kill -9", cfg.sshKeyFile)
                        except Exception as e:
                            print(" ")

                    if cfg.useStorage and not noKillStorage:
                        try:
                            for s in cfg.storageIps:
                                print("Killing Storage" + str(s))
                                executeRemoteCommandNoCheck("%s@%s" %(cfg.user, s), "ps -ef | grep java | grep -v grep | awk '{print \$2}' | xargs -r kill -9", cfg.sshKeyFile)
                        except Exception as e:
                            print(" ")
                    else:
                        print("No Kill Storage")
                    if (deleteTable): deleteDynamoTables(properties)
                #except Exception as e:
                #    print("Error: %s" % e)
                except subprocess.CalledProcessError as e:
                    print(str(e.returncode))
        calculateParallel(propertyFile, localExpDir, clientInstance)

    # Tear down TC rules
    if simulateLatency:
        if cfg.useProxy:
            deleteTC(cfg.proxyIp, cfg.storagePrivateIps, cfg.sshKeyFile)
        if cfg.useStorage:
            for s in cfg.storageIps:
                if cfg.useProxy:
                    deleteTC(s, cfg.proxyPrivateIps, cfg.sshKeyFile)
                else:
                    deleteTC(s, cfg.clientPrivateIps, cfg.sshKeyFile)
            for c in cfg.clientIps:
                deleteTC(s, cfg.storagePrivateIps, cfg.sshKeyFile)
    return cfg.experimentDir


# Collects the data for the experiment
def collectData(propertyFile, localFolder, remoteFolder, currClientIps):
    print("Collecting Data")
    properties = loadPropertyFile(propertyFile)
    user = properties['username']
    sshKeyFile = properties['ssh_key_file']
    useStorage = toBool(properties['usestorage'])
    useProxy = toBool(properties['useproxy'])
    proxyIps = properties['proxy_ips']
    storageIpsByCluster = properties["storage_ips"]
    storageIps = []
    for ips in storageIpsByCluster:
        storageIps.extend(ips)
    getDirectory(localFolder, user, currClientIps, remoteFolder, sshKeyFile)
    if (useProxy):
        getDirectory(localFolder, user, proxyIps, remoteFolder, sshKeyFile)
    if (useStorage):
        getDirectory(localFolder, user, storageIps, remoteFolder,
                sshKeyFile)


# Computes experiment results and outputs all results in results.dat
# For each round in an experiment run the "generateData" method as a separate
# thread
def calculateParallel(propertyFile, localExpDir, clientInstance):
    properties = loadPropertyFile(propertyFile)
    if not properties:
        print("Empty property file, failing")
        return
    nbRounds = len(properties['nbclients'])
    experimentName = properties['experiment_name']
    if (not localExpDir):
            localProjectDir = properties['local_project_dir']
            expDir = properties['experiment_dir']
            localExpDir = localProjectDir / expDir
    threads = list()
    fileHandler = open(localExpDir / "results.dat", "w+")
    for it in range (0, nbRepetitions):
        time = int(properties['exp_length'])
        manager = multiprocessing.Manager()
        results = manager.dict()
        for i in range(0, nbRounds):
            try:
                nbClients = int(properties['nbclients'][i])
                innerFolder = str(nbClients) + "_" + clientInstance + "_" + str(it)
                folderName = localExpDir / innerFolder / innerFolder
                executeCommand(f"rm -f {folderName / 'clients.dat'}")
                fileList = dirList(folderName, False,'dat')
                combineFiles(fileList, str(folderName / "clients.dat"))
                t = multiprocessing.Process(target=generateData,args=(results, str(folderName / "clients.dat"), nbClients, time))
                threads.append(t)
            except:
                print("No File " + folderName)

        executingThreads = list()
        while (len(threads)>0):
           for c in range(0,2):
                try:
                    t = threads.pop(0)
                except:
                    break
                print("Remaining Tasks " + str(len(threads)))
                executingThreads.append(t)
           for t in executingThreads:
                   t.start()
           for t in executingThreads:
                   t.join()
           print("Finished Processing Batch")
           executingThreads = list()
        sortedKeys = sorted(results.keys())
        for key in sortedKeys:
            fileHandler.write(results[key])
        fileHandler.flush()
    fileHandler.close()


# Generates data using the math functions available in math_util
# Expects latency to be in the third column of the output file
def generateData(results,folderName, clients, time):
    print("Generating Data for " + folderName)
    result = str(clients) + " "
    result+= str(computeMean(folderName,2)) + " "
    result+= str(computeMin(folderName,2)) + " "
    result+= str(computeMax(folderName,2)) + " "
    result+= str(computeVar(folderName,2)) + " "
    result+= str(computeStd(folderName,2)) + " "
    result+= str(computePercentile(folderName,2,50)) + " "
    result+= str(computePercentile(folderName,2,75)) + " "
    result+= str(computePercentile(folderName,2,90)) + " "
    result+= str(computePercentile(folderName,2,95)) + " "
    result+= str(computePercentile(folderName,2,99)) + " "
    result+= str(computeThroughput(folderName,2,time,1)) + " \n"
    results[clients]=result


# Plots a throughput-latency graph. This graph assumes the
# data format in calculate() function
# Pass in as argument: a list of tuple (dataName, label)
# and the output to which this should be generated
def plotThroughputLatency(dataFileNames, outputFileName, title = None):
    x_axis = "Throughput(Trx/s)"
    y_axis = "Latency(ms)"
    if (not title):
        title = "Throughput-Latency Graph"
    data = list()
    for x in dataFileNames:
        data.append((x[0], x[1], 11, 1))
    plotLine(title, x_axis,y_axis, outputFileName, data, False, xrightlim=200000, yrightlim=5)


# Plots a throughput. This graph assumes the
# data format in calculate() function
# Pass in as argument: a list of tuple (dataName, label)
# and the output to which this should be generated
def plotThroughput(dataFileNames, outputFileName, title = None):
    x_axis = "Clients"
    y_axis = "Throughput (trx/s)"
    if (not title):
        title = "ThroughputGraph"
    data = list()
    for x in dataFileNames:
        data.append((x[0], x[1], 0, 11))
    plotLine(title, x_axis,y_axis, outputFileName, data, False, xrightlim=300, yrightlim=200000)

# Plots a throughput. This graph assumes the
# data format in calculate() function
# Pass in as argument: a list of tuple (dataName, label)
# and the output to which this should be generated
def plotLatency(dataFileNames, outputFileName, title = None):
    x_axis = "Clients"
    y_axis = "Latency(ms)"
    if (not title):
        title = "LatencyGraph"
    data = list()
    for x in dataFileNames:
        data.append((x[0], x[1], 0, 1))
    plotLine(title, x_axis,y_axis, outputFileName, data, False, xrightlim=300, yrightlim=5)


def main():
    parser = argparse.ArgumentParser(description='Run experiment.')
    parser.add_argument('-p', '--provision', action='store_true',
                        help='provision instances (default: false)')
    parser.add_argument('-s', '--setup', action='store_true',
                        help='setup instances (default: false)')
    parser.add_argument('-r', '--run', action='store_true',
                        help='run experiment (default: false)')
    parser.add_argument('-c', '--cleanup', action='store_true',
                        help='cleanup instances(default: false)')
    args = parser.parse_args()

    if not args.provision and not args.setup and not args.run and not args.cleanup:
        parser.print_help()
        parser.exit()

    if args.provision:
        print("Provisioning...")
        provisionExperiment(PROP_FILE)
    if args.setup:
        if args.provision:
            print("Waiting 30s for provisioned VMs to come online...")
            time.sleep(30)
        print("Setting up...")
        setupExperiment(PROP_FILE)
    if args.run:
        print("Running experiment...")
        runExperiment(PROP_FILE)
    if args.cleanup:
        print("Cleaning up...")
        cleanupExperiment()

if __name__ == '__main__':
    main()
