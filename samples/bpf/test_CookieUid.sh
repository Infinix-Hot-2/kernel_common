#!/bin/bash
local_dir="$(pwd)"
root_dir=$local_dir/../..
mnt_dir=/sys/fs/bpf/perSocketStats

on_exit {
  iptables -D INPUT -m bpf --object-pinned ${mnt_dir}/bpf_prog -j ACCEPT
  umount ${mnt_dir}
}

trap on_exit EXIT
if [ -d ${mnt_dir} ]; then
	echo 'File already exist, aborting'
	exit
fi
mkdir -p ${mnt_dir}
mount -t bpf bpf ${mnt_dir}
./perSocketStats_example mnt_dir/bpf_prog &
PID=$!
iptables -A OUTPUT -t raw -p tcp -d 127.0.0.3 -j MARK --set-mark 4
netperf -4 -t TCP_STREAM -H 127.0.0.3
wait ${PID}
