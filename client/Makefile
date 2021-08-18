# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

include ../../config.mk

.PHONY: all build clean run

all: build

CFLAGS=$(shell pkg-config oehost-$(C_COMPILER) --cflags)
CXXFLAGS=$(shell pkg-config oehost-$(CXX_COMPILER) --cflags)
LDFLAGS=$(shell pkg-config oehost-$(CXX_COMPILER) --libs)

all: build

build:
	$(CXX) -g -c $(CXXFLAGS) $(INCLUDES) -std=c++11 -I. -I/usr/include/openssl client.cpp cert_verify_config.cpp ../common/verify_callback.cpp ../common/crypto.cpp ../common/block.cpp
	$(CXX) -o client client.o cert_verify_config.o verify_callback.o crypto.o block.o config.o $(LDFLAGS)

clean:
	rm -f client *.o ../cert.der

run:
	./client -server:localhost -port:12341
