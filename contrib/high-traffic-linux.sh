#! /bin/sh

sysctl net.ipv4.tcp_max_syn_backlog=8192
sysctl net.core.somaxconn=4096 # default: 128
sysctl net.ipv4.tcp_max_tw_buckets=128000000
sysctl net.ipv4.tcp_fin_timeout=5

# XXX Enable *ONLY* if you're not working with NAT
sysctl net.ipv4.tcp_tw_recycle=1

# XXX The following certainly fixes the "Cannot assign requested address." (EADDRNOTAVAIL) issue,
# but try to avoid this setting on public servers.
sysctl net.ipv4.tcp_tw_reuse=1

# Increases the range of allowed source-ports
#sysctl net.ipv4.ip_local_port_range='1024 65000'
sysctl net.ipv4.ip_local_port_range='32768 61000'

if lsmod | grep -q nf_conntrck; then
	sysctl net.ipv4.netfilter.ip_conntrack_max=750000
	sysctl net.netfilter.nf_conntrack_tcp_timeout_fin_wait=60
	sysctl net.netfilter.nf_conntrack_tcp_timeout_close_wait=30
	sysctl net.netfilter.nf_conntrack_tcp_timeout_time_wait=15
fi
