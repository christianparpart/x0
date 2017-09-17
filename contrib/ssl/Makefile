# This file is part of the "x0" project, http://github.com/christianparpart/x0>
#   (c) 2009-2017 Christian Parpart <christian@parpart.family>
#
# Licensed under the MIT License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of
# the License at: http://opensource.org/licenses/MIT

CERTFILE = cert.pem
KEYFILE = key.pem

all: $(CERTFILE)

cert.pem: cert.conf
	openssl req -config cert.conf \
				-new \
				-x509 \
				-sha256 \
				-nodes \
				-days 365 \
				-newkey rsa:4096 \
				-keyout $(KEYFILE) \
				-out $(CERTFILE)

selfsigned: $(CERTFILE)

install:
	ln -sf $(shell pwd)/$(CERTFILE) /etc/ssl/certs/$(shell openssl x509 -noout -hash -in $(CERTFILE)).0

trust:
	openssl x509 -noout -hash -in $(CERTFILE)

inspect:
	openssl x509 -noout -text -in $(CERTFILE)

clean:
	rm -f $(CERTFILE) $(KEYFILE)

.PHONY: clean inspect selfsigned trust
