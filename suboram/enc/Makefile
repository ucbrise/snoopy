# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

include ../../config.mk

CFLAGS=$(shell pkg-config oeenclave-$(C_COMPILER) --cflags)
CXXFLAGS=$(shell pkg-config oeenclave-$(CXX_COMPILER) --cflags)
LDFLAGS=$(shell pkg-config oeenclave-$(CXX_COMPILER) --libs)
INCDIR=$(shell pkg-config oeenclave-$(C_COMPILER) --variable=includedir)
CRYPTO_LDFLAGS=$(shell pkg-config oeenclave-$(COMPILER) --variable=${OE_CRYPTO_LIB}libs)

.PHONY: all build clean run

SRC_FILES = ecalls.cpp cert_verify_config.cpp ../../common/verify_callback.cpp ../../common/utility.cpp ../../common/openssl_utility.cpp ../../common/crypto.cpp ../../common/block.cpp oblix/AES.cpp oblix/Bid.cpp oblix/LocalRAMStore.cpp oblix/ORAM.cpp oblix/ObliviousOperations.cpp oblix/OMAP.cpp oblix/AVLTree.cpp
OBJ_FILES = ecalls.o cert_verify_config.o verify_callback.o utility.o openssl_utility.o crypto.o block.o aes.o bid.o local_ram_store.o oram.o oblivious_operations.o omap.o avltree.o suboram_t.o

all:
	$(MAKE) build
	$(MAKE) sign

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	oeedger8r ../suboram.edl --trusted \
		--trusted-dir . \
		--search-path $(INCDIR) \
		--search-path $(INCDIR)/openenclave/edl/sgx


	$(CXX) -g -c $(CXXFLAGS) $(INCLUDES) -I. -std=c++11 ${SRC_FILES}
	$(CC) -c $(CFLAGS) $(CINCLUDES) -I. suboram_t.c
	$(CXX) -o suboram_enc $(OBJ_FILES) $(LDFLAGS) $(CRYPTO_LDFLAGS) -loehostsock -loehostresolver

sign:
	oesign sign -e suboram_enc -c enc.conf -k private.pem
	oesign dump -e suboram_enc.signed > temp.dmp
	chmod u+x ../../gen_mrenclave_header.sh
	../../gen_mrenclave_header.sh ../../common/suboram_enc_mrenclave.h temp.dmp
	rm temp.dmp

clean:
	rm -f *.o suboram_enc suboram_enc.signed suboram_enc.signed.so suboram_t.* suboram_args.h yy ../../common/suboram_enc_mrenclave.h
