#! /bin/bash

ulimit -c unlimited

exec sudo ./x0d --no-fork --config=test.conf
