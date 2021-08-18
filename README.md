# Snoopy: A Scalable Oblivious Storage System
Snoopy is a high-throughput oblivious storage system that scales similarly to a plaintext storage system. This implementation contains our Snoopy system as described in the forthcoming SOSP’21 paper.

**WARNING:** This is an academic proof-of-concept prototype and has not received careful code review. This implementation is NOT ready for production use.

This prototype is released under the Apache v2 license (see [License](LICENSE)).

# Artifact Evaluation
## Overview
* [Baselines](#baselines)
    * [Oblix](#oblix) (5 compute-minutes)
    * [Obladi](#obladi) (15 compute-minutes)
    * [Propagate Baselines](#propagate-baselines) (<1 minute)
* [Provisioning Machines for Snoopy](#provisioning-machines-for-snoopy) (5 compute-minutes)
* [Figure 9](#figure-9) (195 compute-minutes)
* [Figure 10a](#figure-10a) (30 compute-minutes)
* [Figure 10b](#figure-10b) (10 compute-minutes)
* [Figure 11a, 11b, 11c](#figure-11a-11b-11c) (?? compute-minutes)
* Figure 12a (2 compute-minutes)
* Figure 12b (12 compute-minutes)
* Figure 13 
* Figure 14
* [Cleanup](#cleanup) (1 human-minute, 10 compute-minutes)
* [Comparing Figures](#comparing-figures) (20 human-minutes)


For our experiments, we will use a cluster of Azure confidential computing instances. Reviewers should have been provided with credentials to our Azure environment.

## Baselines
The Oblix and Obladi baselines can be run in parallel because they use different clusters. We recommend running the commands in two tmux windows.

### Oblix
```sh
cd ~/snoopy/scripts/oblix
python3 runExperiment.py --config=config/oblix_benchmark.json -psrc     # 5 minutes
```
The above command can be broken down as follows:

* `-p` launches a cluster of one Azure SGX VM with an image that has [Oblix](https://people.eecs.berkeley.edu/~raluca/oblix.pdf) installed.
* `-s` sets up a new experiment directory and symlinks it to `~/snoopy/experiments/oblix_benchmark/latest`.
* `-r` runs the Oblix benchmark, which measures the latencies of accessing storages of 10-2M blocks. We use the latency of accessing 2M blocks in Figure 9 (in order to calculate the baseline throughput), Figure 10b (baseline latency), Figure 13 (in order to emulate using Oblix as a subORAM).
* `-c` destroys the cluster. 

### Obladi
```sh
cd ~/snoopy/scripts/obladi
python3 runExperiment.py -psrc      # 15 minutes
```
The above command can be broken down as follows:
* `-p` launches an [Obladi](https://www.usenix.org/system/files/osdi18-crooks.pdf) cluster of two VMs.
* `-s` sets up a new experiment directory and symlinks it to `~/snoopy/experiments/diff-clients-base-oram-par-server-1/latest`. It compiles Obladi and sends the compiled jar files to the remote Obladi cluster.
* `-r` runs the Obladi benchmark, which measures the throughput of accessing a storage containing 2M blocks. We use this throughput to plot Figure 9.
* `-c` destroys the cluster. 

### Propagate Baselines
After running the Obladi and Oblix baselines, run the following command to propagate their values for future figures and experiments:

```sh
cd ~/snoopy/scripts
./process_baselines.sh      # instant
```

## Provisioning Machines for Snoopy
```sh
cd ~/snoopy/scripts
./provision_machines.sh     # 5 minutes
```
The aforementioned command launches a cluster of 21 Azure SGX VMs and updates the experiment config files with the machines' IP addresses. It costs ~$370/per day to run this cluster, so please [destroy](#cleanup) the cluster when you are done with artifact evaluation.

The rest of the instructions assume that your current working directory is `~/snoopy/scripts`.


## Figure 9
```
./figure9.sh
```
<details>
<summary>Sample Figure 9 output</summary>
    
![Sample Figure 9](experiments/artifact_figures/sample/9.png)
</details>

## Figure 10a
```
python3 runExperiment.py -f 10a -srg        # 30 minutes
```
<details>
<summary>Sample Figure 10a output</summary>
    
![Sample Figure 10a](experiments/artifact_figures/sample/10a.png)
</details>

## Figure 10b
```
python3 runExperiment.py -f 10b -srg        # 10 minutes
```
The graph will be located in `~/snoopy/experiments/artifact_figures/10b.pdf`. The artifact evaluation graph is drawn on the left, and the graph in the paper is drawn on the right.

<details>
<summary>Sample Figure 10b output</summary>
    
![Sample Figure 10b](experiments/artifact_figures/sample/10b.png)
</details>

## Figure 11a, 11b, 11c
```
./figure11.sh
```
<details>
<summary>Sample Figure 11a, 11b, 11c output</summary>
    
![Sample Figure 11a](experiments/artifact_figures/sample/11a.png)
![Sample Figure 11b](experiments/artifact_figures/sample/11b.png)
![Sample Figure 11c](experiments/artifact_figures/sample/11c.png)
</details>

## Figure 12a
```
python3 runExperiment.py -f 12a -srg
```
<details>
<summary>Sample Figure 12a output</summary>
    
![Sample Figure 12a](experiments/artifact_figures/sample/12a.png)
</details>

## Figure 12b
<details>
<summary>Sample Figure 12b output</summary>
    
![Sample Figure 12b](experiments/artifact_figures/sample/12b.png)
</details>

## Figure 13
```
./figure13.sh
```
<details>
<summary>Sample Figure 13 output</summary>
    
![Sample Figure 13](experiments/artifact_figures/sample/13.png)
</details>

## Figure 14
**PREREQUISITE: Requires Figure 11a, 11b, and 11c.**
<details>
<summary>Sample Figure 14 output</summary>
    
![Sample Figure 14](experiments/artifact_figures/sample/14.png)
</details>

## Cleanup
```
python3 runExperiment.py -c     # 10 minutes
```
This command destroys the cluster. **Please remember to run this when you are done with artifact evaluation.** It costs ~$370/per day to run the cluster!

## Comparing Figures
All figures produced by this artifact are located in `~/snoopy/experiments/artifact_figures`. We recommend scp'ing this folder to your local computer so you can easily view each PDF. In each PDF, the artifact evaluation graph is drawn on the left and the graph in the paper is drawn on the right.

<details>
<summary>Sample Figure 9 output</summary>
    
![Sample Figure 9](experiments/artifact_figures/sample/9.png)
</details>

<details>
<summary>Sample Figure 10a output</summary>
    
![Sample Figure 10a](experiments/artifact_figures/sample/10a.png)
</details>

<details>
<summary>Sample Figure 10b output</summary>
    
![Sample Figure 10b](experiments/artifact_figures/sample/10b.png)
</details>

<details>
<summary>Sample Figure 11a, 11b, 11c output</summary>
    
![Sample Figure 11a](experiments/artifact_figures/sample/11a.png)
![Sample Figure 11b](experiments/artifact_figures/sample/11b.png)
![Sample Figure 11c](experiments/artifact_figures/sample/11c.png)
</details>

<details>
<summary>Sample Figure 12a output</summary>
    
![Sample Figure 12a](experiments/artifact_figures/sample/12a.png)
</details>

<details>
<summary>Sample Figure 12b output</summary>
    
![Sample Figure 12b](experiments/artifact_figures/sample/12b.png)
</details>

<details>
<summary>Sample Figure 13 output</summary>
    
![Sample Figure 13](experiments/artifact_figures/sample/13.png)
</details>

<details>
<summary>Sample Figure 14 output</summary>
    
![Sample Figure 14](experiments/artifact_figures/sample/14.png)
</details>

# Playing with Snoopy
## Building Snoopy Locally
Protobuf:
```sh
# On ubuntu 18.04
sudo apt-get install build-essential autoconf libtool pkg-config automake zlib1g-dev
cd third_party
git submodule update --init
cd protobuf/cmake
mkdir build
cd build
cmake -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd`/../install ..
make -j `nproc`
make install
```

gRPC (requires protobuf first):
```sh
cd grpc
git submodule update --init
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=`pwd`/../../protobuf/cmake/install -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_PROTOBUF_PROVIDER=package -DgRPC_ZLIB_PROVIDER=package -DgRPC_CARES_PROVIDER=module -DgRPC_SSL_PROVIDER=package \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=`pwd`/install \
      ../
make
make install
```

Need to put boost library in `/usr/local/` (doesn't need to be built or installed).

Running anything in the `scripts` directory:

Follow instructions here: https://docs.microsoft.com/en-us/azure/developer/python/configure-local-development-environment?tabs=cmd
```sh
pip install -r requirements.txt
export AZURE_SUBSCRIPTION_ID=<azure-subscription-id>
```

## Debugging

- Make sure DEBUG is turned on in enc.conf and OE_FLAG_DEBUG is passed into enclave creation

- Make debug build:
```
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

Run oegdb as usual:
```sh
/opt/openenclave/bin/oegdb --args ...
```

Handle SIGILL:
```sh
(gdb) handle SIGILL nostop
```
