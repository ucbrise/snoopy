import math
from collections import defaultdict
from scipy.special import lambertw

lb_1_name = "bench/micro_balancer_make_batch.dat"
lb_2_name = "bench/micro_balancer_match_resps.dat"
suboram_name = "bench/micro_suboram_batch_sz.dat"

suboram_cost = 577.43
lb_cost = 577.43

max_suborams = 10
max_lbs = 10

def getLoadBalancerData():
    results = []
    f1 = open(lb_1_name, "r")
    f2 = open(lb_2_name, "r")
    lines_1 = f1.readlines()
    lines_2 = f2.readlines()
    for i in range(len(lines_1)):
        elems_1 = lines_1[i].split()
        elems_2 = lines_2[i].split()
        result = {
            "suborams": int(elems_1[0]),
            "requests": int(elems_1[1]),
            "latency": (float(elems_1[2]) + float(elems_2[2])) / 1000000.0,
            }
        results.append(result)
    f1.close()
    f2.close()
    return results

def getSuboramData():
    results = []
    with open(suboram_name, "r") as f:
        lines = f.readlines()
        for line in lines:
            elems = line.split()
            result = {
                "data_size": int(elems[0]),
                "batch": int(elems[1]),
                "latency": float(elems[2]) / 1000.0,
                }
            results.append(result)
    return results

def getLoadBalancerLatencyForParams(data, suborams, requests):
    for elem in data:
        if elem["suborams"] == suborams and elem["requests"] == requests:
            return elem["latency"]
    print(("out-of-bounds params: no latency for params suborams=%d, requests=%d") % (suborams, requests))
    return -1.0

def getSuboramLatencyForParams(data, data_size, batch):
    for elem in data:
        if elem["data_size"] == data_size and elem["batch"] == batch:
            return elem["latency"]
    print(("out-of-bounds params: no latency for params data_size=%d, batch=%d") % (data_size, batch))
    return -1.0



def f(N, n_suborams, secparam=128):
    mu = N / n_suborams
    alpha = math.log(n_suborams * (2 ** secparam))
    rhs = alpha / (math.e * mu) - 1 / math.e
    branch = 0 
    epsilon = math.e ** (lambertw(rhs, branch) + 1) - 1 
    #epsilon = (alpha + math.sqrt(2 * mu * alpha)) / mu     # uncomment for looser bound
    #print(alpha, rhs, lambertw(rhs, 0), lambertw(rhs, 1))
    #print("bound", suborams, secparam, alpha, rhs, lambertw(rhs), epsilon)
    return mu * (1 + epsilon)

def getSysCost(suborams, balancers):
    return (float(suborams) * suboram_cost) + (float(balancers) * lb_cost)

def getEpochTime(latency):
    return 2.0 * float(latency) / 5.0 

def roundUpPow2(x):
        
    up = 2 ** (math.ceil(math.log(x,2)))
    down = 2 ** (math.floor(math.log(x,2)))
    if abs(x - up) < abs(x - down):
        return up
    else:
        return down
    
    
    
    #return 2 ** (math.floor(math.log(x,2)))
    #return 2 ** (math.ceil(math.log(x,2)))

def checkIfReachesThroughputForParams(suboram_data, lb_data, suborams, lbs, latency, data_size, target_throughput):
    epoch_time = getEpochTime(latency)
    reqs_per_epoch = target_throughput * epoch_time
    reqs_per_epoch_rounded = roundUpPow2(reqs_per_epoch)
    reqs_per_lb_epoch_rounded = roundUpPow2(reqs_per_epoch / float(lbs))
    print(("Reqs per epoch = %d, rounded up from %d") % (reqs_per_epoch_rounded, reqs_per_epoch))
    batch_size = f(reqs_per_epoch, suborams)
    batch_size_rounded = roundUpPow2(batch_size)
    print(("Batch size = %d, rounded up from %d") % (batch_size, batch_size_rounded))
    data_size_per_suboram = data_size / suborams
    data_size_per_suboram_rounded = roundUpPow2(data_size / suborams)
    print(("Data size per suboram = %d, rounded up from %d") % (data_size_per_suboram, data_size_per_suboram_rounded))
    if batch_size_rounded > data_size_per_suboram_rounded:
        batch_size_rounded = data_size_per_suboram_rounded
    suboram_time = float(lbs) * getSuboramLatencyForParams(suboram_data, data_size_per_suboram_rounded, batch_size_rounded)
    if suboram_time < 0:
        return False
    print(("Suboram time: %f s") % (suboram_time))
    lb_time = getLoadBalancerLatencyForParams(lb_data, suborams, reqs_per_lb_epoch_rounded)
    if lb_time < 0:
        return False
    print(("Load balancer time: %f s") % (lb_time))
    computed_time = max(lb_time, suboram_time)
    print(("Epoch time %f, computed epoch time %f") % (epoch_time, computed_time))
    return computed_time <= epoch_time
 
# latency in seconds, throughput reqs/sec, data_size in number of blocks (should be pow of 2 between 8192 and 16777216
def getConfigMinCost(latency, throughput, data_size):
    lb_data = getLoadBalancerData()
    suboram_data = getSuboramData()
    best_config = {
        "suborams": -1,
        "load_balancers": -1,
        "cost": 100000000
    }
    for i in range(max_suborams):
        for j in range(max_lbs):
            suborams = i + 1
            lbs = j + 1
            print(("suborams=%d, load_balancer=%d") % (suborams, lbs))
            reaches_throughput = checkIfReachesThroughputForParams(suboram_data, lb_data, suborams, lbs, latency, data_size, throughput)
            system_cost = getSysCost(suborams, lbs)
            if reaches_throughput and best_config["cost"] > system_cost:
                best_config = {
                    "suborams": suborams,
                    "load_balancers": lbs,
                    "cost": system_cost
                }

    return best_config

#config = getConfigMinCost(1.0, 10000.0, 16384)
config = getConfigMinCost(1.0, 50000.0, 16384)
print(config)

