#! /usr/bin/rrdcgi
<!-- 2003-2009, Written by Christian Parpart (trapni@gentoo.org) -->
<HTML>
  <HEAD><TITLE>RRD x0d Statistics</TITLE></HEAD>
  <BODY>
    <H1>x0d statistics:</H1>
    <P>
      <RRD::GRAPH /home/trapni/projects/x0/www/htdocs/rrd/x0d-hour.gif
        --imginfo '<IMG SRC="/rrd/x0d-hour.gif">'
        --lazy --title="(hour)"
        --start -90m
        DEF:requests=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:requests:AVERAGE
        DEF:bytes_in=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_in:AVERAGE
        DEF:bytes_out=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_out:AVERAGE
        LINE1:requests#CC0000:"requests"
        LINE1:bytes_in#00aa00:"bytes_in"
        LINE1:bytes_out#0000FF:"bytes_out">
    </P>
    <P>
      <RRD::GRAPH /home/trapni/projects/x0/www/htdocs/rrd/x0d-day.gif
        --imginfo '<IMG SRC="/rrd/x0d-day.gif">'
        --lazy --title="(day)"
        --start -36h
        DEF:requests=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:requests:AVERAGE
        DEF:bytes_in=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_in:AVERAGE
        DEF:bytes_out=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_out:AVERAGE
        LINE1:requests#CC0000:"requests"
        LINE1:bytes_in#00aa00:"bytes_in"
        LINE1:bytes_out#0000FF:"bytes_out">
    </P>
    <P>
      <RRD::GRAPH /home/trapni/projects/x0/www/htdocs/rrd/x0d-week.gif
        --imginfo '<IMG SRC="/rrd/x0d-week.gif">'
        --lazy --title="(week)"
        --start -10d
        DEF:requests=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:requests:AVERAGE
        DEF:bytes_in=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_in:AVERAGE
        DEF:bytes_out=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_out:AVERAGE
        LINE1:requests#CC0000:"requests"
        LINE1:bytes_in#00aa00:"bytes_in"
        LINE1:bytes_out#0000FF:"bytes_out Spawned">
    </P>
    <P>
      <RRD::GRAPH /home/trapni/projects/x0/www/htdocs/rrd/x0d-month.gif
        --imginfo '<IMG SRC="/rrd/x0d-month.gif">'
        --lazy --title="(month)"
        --start -6w
        DEF:requests=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:requests:AVERAGE
        DEF:bytes_in=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_in:AVERAGE
        DEF:bytes_out=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_out:AVERAGE
        LINE1:requests#CC0000:"requests"
        LINE1:bytes_in#00aa00:"Users bytes_in"
        LINE1:bytes_out#0000FF:"bytes_out Spawned">
    </P>
    <P>
      <RRD::GRAPH /home/trapni/projects/x0/www/htdocs/rrd/x0d-year.gif
        --imginfo '<IMG SRC="/rrd/x0d-year.gif">'
        --lazy --title="(year)"
        --start -549d
        DEF:requests=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:requests:AVERAGE
        DEF:bytes_in=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_in:AVERAGE
        DEF:bytes_out=/home/trapni/projects/x0/www/htdocs/rrd/x0d.rrd:bytes_out:AVERAGE
        LINE1:requests#CC0000:"requests"
        LINE1:bytes_in#00aa00:"bytes_in"
        LINE1:bytes_out#0000FF:"bytes_out">
    </P>
    <HR>
  </BODY>
</HTML>
<!-- vim:ai:et:ts=2:nowrap
  -->
