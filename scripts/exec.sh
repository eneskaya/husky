# E.g.,
#
# worker1
# worker2
# worker3
#
MACHINE_CFG=/var/nfs/general/husky/machines

# This point to the directory where Husky binaries live.
# If Husky is running in a cluster, this directory should be available
# to all machines.
BIN_DIR=/home/bigdata/kaya/husky/release
time parallel-ssh -t 0 -P -h ${MACHINE_CFG} -x "-t -t" "cd $BIN_DIR && ./$@"
