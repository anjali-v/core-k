cd /sys/fs/cgroup/cpuset/
sudo mkdir rt nrt
echo 0,1 | sudo tee  nrt/cpuset.cpus 
echo 2,3 | sudo tee  rt/cpuset.cpus 
echo 1   | sudo tee  rt/cpuset.cpu_exclusive 
echo 0   | sudo tee nrt/cpuset.mems 
echo 0   | sudo tee rt/cpuset.mems
echo 0   | sudo tee rt/cpuset.mems
echo 0   | sudo tee cpuset.sched_load_balance 
echo 0   | sudo tee rt/cpuset.sched_load_balance 
echo 1   | sudo tee nrt/cpuset.sched_load_balance 
