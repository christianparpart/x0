#! /bin/bash

# this script is used to test client aborts for cgi and proxy plugin

function term_handler()
{
	echo "$(date): terminated" >> cgi.log
	exit 42
}

echo -ne "Status: 200\r\n"
echo -ne "Content-Type: text/plain\r\n"
echo -ne "\r\n"

trap term_handler TERM INT

for ((i=0; i<10; i++)); do
	echo -ne "i = $i\r\n"
	sleep 2
done
