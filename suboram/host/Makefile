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
	oeedger8r ../suboram.edl --untrusted \
		--search-path $(INCDIR) \
		--search-path $(INCDIR)/openenclave/edl/sgx
	$(CC) -c $(CFLAGS) $(CINCLUDES) suboram_u.c
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) host.cpp ../../common/crypto.cpp ../../common/block.cpp oblix/RAMStore.cpp oblix/Utilities.cpp
	$(CXX) -o suboram_host host.o crypto.o block.o ram_store.o utilities.o suboram_u.o $(LDFLAGS)

clean:
	rm -f suboram_host* *.o suboram_u.* suboram_args.h
