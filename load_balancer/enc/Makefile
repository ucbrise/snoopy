# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

include ../../config.mk

CFLAGS=$(shell pkg-config oeenclave-$(C_COMPILER) --cflags)
CXXFLAGS=$(shell pkg-config oeenclave-$(CXX_COMPILER) --cflags)
LDFLAGS=$(shell pkg-config oeenclave-$(CXX_COMPILER) --libs)
INCDIR=$(shell pkg-config oeenclave-$(C_COMPILER) --variable=includedir)
CRYPTO_LDFLAGS=$(shell pkg-config oeenclave-$(COMPILER) --variable=${OE_CRYPTO_LIB}libs)

.PHONY: all build clean run

SRC_FILES = ecalls.cpp openssl_server.cpp balancer.cpp test_balancer.cpp obl_primitives.cpp cert_verify_config.cpp ../../common/verify_callback.cpp ../../common/utility.cpp ../../common/openssl_utility.cpp ../../common/crypto.cpp ../../common/block.cpp
OBJ_FILES = ecalls.o openssl_server.o balancer.o test_balancer.o obl_primitives.o cert_verify_config.o verify_callback.o utility.o openssl_utility.o load_balancer_t.o crypto.o block.o

all:
	$(MAKE) build
	$(MAKE) sign

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	oeedger8r ../load_balancer.edl --trusted \
		--trusted-dir . \
		--search-path $(INCDIR) \
		--search-path $(INCDIR)/openenclave/edl/sgx


	$(CXX) -g -c $(CXXFLAGS) $(INCLUDES) -I. -std=c++11 ${SRC_FILES}
	$(CC) -c $(CFLAGS) $(CINCLUDES) -I. load_balancer_t.c
	$(CXX) -o load_balancer_enc $(OBJ_FILES) $(LDFLAGS) $(CRYPTO_LDFLAGS) -loehostsock -loehostresolver

sign:
	oesign sign -e load_balancer_enc -c enc.conf -k private.pem
	oesign dump -e load_balancer_enc.signed > temp.dmp
	chmod u+x ../../gen_mrenclave_header.sh
	../../gen_mrenclave_header.sh ../../common/load_balancer_enc_mrenclave.h temp.dmp
	rm temp.dmp

clean:
	rm -f *.o load_balancer_enc load_balancer_enc.signed load_balancer_enc.signed.so load_balancer_t.* load_balancer_args.h yy ../../common/load_balancer_enc_mrenclave.h
