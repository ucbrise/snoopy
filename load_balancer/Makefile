# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

all: build

build:
	$(MAKE) -C enc
	$(MAKE) -C host

clean:
	$(MAKE) -C enc clean
	$(MAKE) -C host clean

run:
	host/load_balancer_host ./enc/load_balancer_enc.signed -port:12341 -suboram_name:localhost -suboram_port:12346
