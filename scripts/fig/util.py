import json
import math
import random
from collections import defaultdict
from scipy.special import lambertw

def parseData(filename):
    results = []
    f = open(filename, "r")
    for line in f:
        elems = line.split()
        result = {
            "clients": int(elems[0]),
            "data_size": int(elems[1]),
            "suborams": int(elems[2]),
            "iter": int(elems[3]),
            "balancers": int(elems[4]),
            "mean_latency": float(elems[6]),
            "min_latenecy": float(elems[7]),
            "max_latency": float(elems[8]),
            "var_latency": float(elems[9]),
            "std_latency": float(elems[10]),
            "50_latency": float(elems[11]),
            "75_latency": float(elems[12]),
            "90_latency": float(elems[13]),
            "95_latency": float(elems[14]),
            "99_latency": float(elems[15]),
            "throughput": float(elems[16])
            }
        results.append(result)
    return results

def parseDataNew(filename):
    results = []
    f = open(filename, "r")
    for line in f:
        elems = line.split()
        result = {
            "clients": int(elems[0]),
            "data_size": int(elems[1]),
            "suborams": int(elems[2]),
            "balancers": int(elems[3]),
            "iter": int(elems[4]),
            "mean_latency": float(elems[5]),
            "min_latenecy": float(elems[6]),
            "max_latency": float(elems[7]),
            "var_latency": float(elems[8]),
            "std_latency": float(elems[9]),
            "50_latency": float(elems[10]),
            "75_latency": float(elems[11]),
            "90_latency": float(elems[12]),
            "95_latency": float(elems[13]),
            "99_latency": float(elems[14]),
            "throughput": float(elems[15])
            }
        results.append(result)
    return results

def parseDataNew2(filename):
    results = []
    f = open(filename, "r")
    for line in f:
        elems = line.split()
        result = {
            "clients": int(elems[0]),
            "data_size": int(elems[1]),
            "suborams": int(elems[2]),
            "balancers": int(elems[3]),
            "epoch_ms": int(elems[4]),
            "iter": int(elems[5]),
            "mean_latency": float(elems[6]),
            "min_latenecy": float(elems[7]),
            "max_latency": float(elems[8]),
            "var_latency": float(elems[9]),
            "std_latency": float(elems[10]),
            "50_latency": float(elems[11]),
            "75_latency": float(elems[12]),
            "90_latency": float(elems[13]),
            "95_latency": float(elems[14]),
            "99_latency": float(elems[15]),
            "throughput": float(elems[16])
            }
        results.append(result)
    return results

def getMaxThroughputForNumBalancers(results, num_balancers):
    ret = 0
    for result in results:
        if result["balancers"] == num_balancers:
            ret = max(ret, result["throughput"])
    return ret

def getMaxThroughputForNumBalancersWithMaxLatency(results, num_balancers, max_latency, suborams=None):
    ret = 0
    for result in results:
        if result["balancers"] == num_balancers and result["90_latency"] <= max_latency:
            if suborams is None or result["suborams"] == suborams:
                ret = max(ret, result["throughput"])
    return ret

def getMaxThroughputForNumBalancersWithMaxMeanLatency(results, num_balancers, max_latency, suborams=None):
    ret = 0
    for result in results:
        if result["balancers"] == num_balancers and result["50_latency"] <= max_latency:
            if suborams is None or result["suborams"] == suborams:
                ret = max(ret, result["throughput"])
    return ret



def getLatencyForMaxThroughputForNumBalancers(results, num_balancers):
    throughput = 0
    ret = 0
    for result in results:
        if result["balancers"] == num_balancers:
            if (throughput < result["throughput"]):
                throughput = result["throughput"]
                ret = result["mean_latency"]
    return ret

def getMaxThroughputForEpochMs(results, epoch_ms):
    ret = 0
    for result in results:
        if result["epoch_ms"] == epoch_ms:
            ret = max(ret, result["throughput"])
    return ret


def getMaxDataForNumSuborams(results, num_suborams, max_latency, latency_type):
    ret =  0
    for result in results:
        if result["suborams"] == num_suborams and result[latency_type] < max_latency:
            print(("Acceptable latency for %d suborams: %d") % (result["suborams"], result[latency_type]))
            ret = max(ret, result["data_size"])
    return ret

def getTupleListOfVals(results, *labels):
    ret = []
    for result in results:
        res = ()
        for l in labels:
            res += (result[l],)
        if res not in ret:
            ret.append(res)
    return ret

def getListOfVals(results, label):
    ret = []
    for result in results:
        if result[label] not in ret:
            ret.append(result[label])
    return ret

def getLatencyForSuboramAndDataSize(results, num_suborams, data_size, latency_type):
    for result in results:
        if result["suborams"] == num_suborams and result["data_size"] == data_size:
            return result[latency_type]

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

def hash_requests(reqs, n_suborams, run):
    offset = run * reqs
    secret = b'Sixteen byte key'
    buckets = defaultdict(int)
    for i in range(offset, offset+reqs):
        """ 
        cobj = CMAC.new(secret, ciphermod=AES)
        cobj.update(i.to_bytes(i.bit_length(), 'big'))
        h = int(cobj.hexdigest(), 16)
        """
        h = int(random.random() * n_suborams)
        bucket = h % n_suborams
        buckets[bucket] += 1
    return max(buckets.values())

def max_requests(n_suborams, target, secparam):
    """ 
    Get maximum request batch size for a given # of suborams that each support target requests.
    """
    l = n_suborams
    r = 2 ** 32
    m = 0
    while l <= r:
        m = math.floor((l+r)/ 2)
        bound = f(m, n_suborams, secparam)
        if bound > target:
            r = m - 1
        elif bound < target:
            l = m + 1
        else:
            return m
    return m

def parse_args(parser):
    parser.add_argument('input', type=str, help='input data')
    parser.add_argument('output', type=str, help='output file')
    parser.add_argument('-b', '--baseline', help='baseline data')
    parser.add_argument('-t', '--title', help='set graph title')
    parser.add_argument('-l', '--large', action='store_true',
                        help='output large graph (default: false)')
    args = parser.parse_args()
    return args

def parse_baseline(filename):
    with open(filename, 'r') as f:
        baseline = json.load(f)
    return baseline