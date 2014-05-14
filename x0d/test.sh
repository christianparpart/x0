#! /bin/bash

ulimit -c unlimited

exec `dirname $0`/src/x0d --no-fork --log-target=console --log-severity=diag -f `dirname $0`/test.conf
