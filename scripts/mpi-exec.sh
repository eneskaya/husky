# This points to a file, which should contains hostnames (one per line).
# E.g.,
#
# worker1
# worker2
# worker3
#
MACHINE_CFG=/home/ubuntu/machines

# Number of machines (workers)
MACHINE_NUM=1
time mpiexec -np ${MACHINE_NUM} -mca plm_rsh_no_tree_spawn 1 -pernode -hostfile ${MACHINE_CFG} $@