# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

include ../../config.mk

CFLAGS=$(shell pkg-config oehost-$(C_COMPILER) --cflags)
CXXFLAGS=$(shell pkg-config oehost-$(CXX_COMPILER) --cflags)
LDFLAGS=$(shell pkg-config oehost-$(CXX_COMPILER) --libs)
INCDIR=$(shell pkg-config oehost-$(C_COMPILER) --variable=includedir)

all: build

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	oeedger8r ../load_balancer.edl --untrusted \
		--search-path $(INCDIR) \
		--search-path $(INCDIR)/openenclave/edl/sgx
	$(CC) -c $(CFLAGS) $(CINCLUDES) load_balancer_u.c
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) host.cpp ../../common/crypto.cpp ../../common/block.cpp
	$(CXX) -o load_balancer_host host.o load_balancer_u.o $(LDFLAGS)

clean:
	rm -f load_balancer_host* *.o load_balancer_u.* load_balancer_args.h
