#! /bin/bash

ulimit -c unlimited

exec ./x0d --no-fork --config=test.conf
