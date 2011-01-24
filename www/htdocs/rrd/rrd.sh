#! /bin/bash

png="x0d.png"
rrd="x0d.rrd"

function create()
{
	rrdtool create ${rrd} --step 60 \
		DS:requests:GAUGE:120:U:U \
		DS:bytes_in:GAUGE:120:U:U \
		DS:bytes_out:GAUGE:120:U:U \
		RRA:AVERAGE:0.5:1:2160 \
		RRA:AVERAGE:0.5:30:480 \
		RRA:AVERAGE:0.5:60:1008 \
		RRA:AVERAGE:0.5:360:2136
}

function graph()
{
	rrdtool graph ${png} \
		DEF:requests=${rrd}:requests:AVERAGE \
		DEF:bytes_in=${rrd}:bytes_in:AVERAGE \
		DEF:bytes_out=${rrd}:bytes_out:AVERAGE \
		"LINE1:requests#ff0000:Requests per Minute" \
		"LINE1:bytes_in#00ff00:Bytes in" \
		"LINE1:bytes_out#0000ff:Bytes Out" \
		--title "x0d client request activity"
}

case $1 in
	create) create ;;
	graph) graph ;;
	*) echo "syntax error" ;;
esac
