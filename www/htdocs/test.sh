#! /bin/bash

echo "Hello: World"
echo "Content-Type: text/plain"
echo ""
echo "listing open FDs of self ($$)"
ls -hl /proc/$$/fd/
for i in `seq 8 -1 1`; do
	echo "poke $i"
	sleep 1;
done
echo "Hello, World"
