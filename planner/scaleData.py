import planner

data_size = 2**15
latency = 10
'''
configs = set()
throughputs = dict()
for throughput in range(100, 100000, 100):
    #data_size = 2**i
    config = planner.getConfigMinCost(latency, throughput, data_size)
    configs.add((config["suborams"], config["load_balancers"]))
    if (config["suborams"],config["load_balancers"]) not in throughputs:
        throughputs[(config["suborams"], config["load_balancers"])] = throughput

print(throughputs)
print(configs)
'''
print("bucket sizes")
print(planner.f(1024,128))
print(planner.f(1024,256))
print(planner.f(1024,512))
print(planner.f(1024,1024))
print(planner.f(1024,2048))
print("2048")
print(planner.f(2048,128))
print(planner.f(2048,256))
print(planner.f(2048,512))
print(planner.f(2048,1024))
print(planner.f(2048,2048))
print(planner.f(2048,4096))
print("4096")
print(planner.f(4096,128))
print(planner.f(4096,256))
print(planner.f(4096,512))
print(planner.f(4096,1024))
print(planner.f(4096,2048))
print(planner.f(4096,4096))
print(planner.f(4096,8192))
print("8192")
print(planner.f(8192,128))
print(planner.f(8192,256))
print(planner.f(8192,512))
print(planner.f(8192,1024))
print(planner.f(8192,2048))
print(planner.f(8192,4096))
print(planner.f(8192,8192))
print(planner.f(8192,16384))
