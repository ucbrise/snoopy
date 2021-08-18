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
	host/suboram_host ./enc/suboram_enc.signed -port:12342 -num_blocks:128
