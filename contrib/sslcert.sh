#! /bin/bash

#openssl req -new -newkey rsa:2048 -nodes -keyout server.key -out server.csr
openssl req -x509 -nodes -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365

