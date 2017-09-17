#! /bin/bash

set -e

configfile="cert.conf"

inspect_cert() {
  openssl x509 -noout -text -in cert.pem | less
}

openssl req -config cert.conf \
            -new \
            -x509 \
            -sha256 \
            -nodes \
            -days 365 \
            -newkey rsa:4096 \
            -keyout key.pem \
            -out cert.pem
