import os
from datetime import datetime

now = datetime.now()
stats_file_name = "MQT_benchmark_statistics_"
stats_file_name += now.strftime("%m-%d-%Y_%H:%M:%S")

directory = os.fsencode("MQT_benchmark_QIR")
stats_file = open(stats_file_name, "a+")

# for file in directory
for file in os.listdir(directory):
    filename = os.fsdecode(file)
    if filename.endswith(".ll"):
        # map gates to number of occurances
        gates = {}
        f = open("MQT_benchmark_QIR" + "/" + filename, "r")
        for line in f:
            if "declare" in line:
                continue;
            if "__quantum__qis__" in line:
                gate = line[line.find("@"):line.find("(")]
                if gate in gates:
                    gates[gate] += 1
                else:
                    gates[gate] = 1
        f.close()
        # save data to gathering file
        stats_file.write(filename + "\n")
        for key in gates:
            stats_file.write("\t" + key + ": " + str(gates[key]) + "\n")
stats_file.close()
