#! /bin/sh

sysctl net.ipv4.tcp_max_syn_backlog=8192
sysctl net.core.somaxconn=4096 # default: 128
sysctl net.ipv4.tcp_max_tw_buckets=128000000
sysctl net.ipv4.tcp_fin_timeout=5

if lsmod | grep -q nf_conntrck; then
	sysctl net.ipv4.netfilter.ip_conntrack_max=750000
	sysctl net.netfilter.nf_conntrack_tcp_timeout_fin_wait=60
	sysctl net.netfilter.nf_conntrack_tcp_timeout_close_wait=30
	sysctl net.netfilter.nf_conntrack_tcp_timeout_time_wait=15
fi
