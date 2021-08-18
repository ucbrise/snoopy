import planner
from operator import itemgetter

data_size = 100 *10e3
#data_size = 2**25 
latency = 1

configs_1 = set()
throughputs_1 = dict()
for throughput in range(100, 150000, 100):
    config = planner.getConfigMinCost(latency, throughput, data_size)
    pair = (config["suborams"], config["load_balancers"])
    configs_1.add(pair)
    if pair not in throughputs_1:
        throughputs_1[pair] = throughput

data_size = 10*10e3
#data_size = 2**20


configs_2 = set()
throughputs_2 = dict()
for throughput in range(100, 150000, 100):
    config = planner.getConfigMinCost(latency, throughput, data_size)
    pair = (config["suborams"], config["load_balancers"])
    configs_2.add(pair)
    if pair not in throughputs_2:
        throughputs_2[pair] = throughput

data_size = 1*10e3
#data_size = 2**15


configs_3 = set()
throughputs_3 = dict()
for throughput in range(100, 150000, 100):
    config = planner.getConfigMinCost(latency, throughput, data_size)
    pair = (config["suborams"], config["load_balancers"])
    configs_3.add(pair)
    if pair not in throughputs_3:
        throughputs_3[pair] = throughput


with open("scale_throughput_config.dat","w") as f:
    configs_1_list = list(configs_1)
    sorted_configs_1_list = sorted(configs_1_list,key=lambda x: (x[0], x[1]))
    for config in sorted_configs_1_list:
        if config[0] < 0 or config[1] < 0:
            continue
        f.write(("config1 %d %d %d\n") % (config[0], config[1], throughputs_1[config]))
   
    '''
    configs_2_list = list(configs_2)
    sorted_configs_2_list = sorted(configs_2_list,key=lambda x: (x[0], x[1]))
    for config in sorted_configs_2_list:
        if config[0] < 0 or config[1] < 0:
            continue
        f.write(("config2 %d %d %d\n") % (config[0], config[1], throughputs_2[config]))
    '''
    

    configs_3_list = list(configs_3)
    sorted_configs_3_list = sorted(configs_3_list,key=lambda x: (x[0], x[1]))
    for config in sorted_configs_3_list:
        if config[0] < 0 or config[1] < 0:
            continue
        f.write(("config3 %d %d %d\n") % (config[0], config[1], throughputs_3[config]))

with open("scale_throughput_cost.dat","w") as f:
    configs_1_list = list(configs_1)
    sorted_configs_1_list = sorted(configs_1_list,key=lambda x: (x[0], x[1]))
    for config in sorted_configs_1_list:
        if config[0] < 0 or config[1] < 0:
            continue
        f.write(("config1 %d %d\n") % (throughputs_1[config], planner.getSysCost(config[0], config[1])))
    
    '''
    configs_2_list = list(configs_2)
    sorted_configs_2_list = sorted(configs_2_list,key=lambda x: (x[0], x[1]))
    for config in sorted_configs_2_list:
        if config[0] < 0 or config[1] < 0:
            continue
        f.write(("config2 %d %d\n") % (throughputs_2[config], planner.getSysCost(config[0], config[1])))
    '''
    

    configs_3_list = list(configs_3)
    sorted_configs_3_list = sorted(configs_3_list,key=lambda x: (x[0], x[1]))
    for config in sorted_configs_3_list:
        if config[0] < 0 or config[1] < 0:
            continue
        f.write(("config3 %d %d\n") % (throughputs_3[config], planner.getSysCost(config[0], config[1])))

