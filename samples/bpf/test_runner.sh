#!/bin/bash
local_dir="$(pwd)"
root_dir=$local_dir/../.. 
on_exit() {
  iptables -F INPUT
  umount /mnt/bpf
}

trap on_exit
mkdir -p /mnt/bpf
mount -t bpf bpf /mnt/bpf

netperf -4 -t TCP_STREAM -H 127.0.0.1
iptables -A OUTPUT -t raw -p tcp -d 127.0.0.3 -j MARK --set-mark 4
netperf -4 -t TCP_STREAM -H 127.0.0.3
./helper_bpf

