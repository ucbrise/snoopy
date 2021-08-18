
<img align="right" src="" alt="Snoopy" width="125"/>

# Snoopy

## Building Snoopy Locally
Protobuf:
```sh
# On ubuntu 18.04
sudo apt-get install build-essential autoconf libtool pkg-config automake zlib1g-dev
cd third_party
git submodule update --init
cd protobuf/cmake
mkdir build
cd build
cmake -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd`/../install ..
make -j `nproc`
make install
```

gRPC (requires protobuf first):
```sh
cd grpc
git submodule update --init
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=`pwd`/../../protobuf/cmake/install -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_PROTOBUF_PROVIDER=package -DgRPC_ZLIB_PROVIDER=package -DgRPC_CARES_PROVIDER=module -DgRPC_SSL_PROVIDER=package \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=`pwd`/install \
      ../
make
make install
```

Need to put boost library in `/usr/local/` (doesn't need to be built or installed).

Running anything in the `scripts` directory:

Follow instructions here: https://docs.microsoft.com/en-us/azure/developer/python/configure-local-development-environment?tabs=cmd
```sh
pip install -r requirements.txt
export AZURE_SUBSCRIPTION_ID=<azure-subscription-id>
```

## Debugging

- Make sure DEBUG is turned on in enc.conf and OE_FLAG_DEBUG is passed into enclave creation

- Make debug build:
```
mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

Run oegdb as usual:
```sh
/opt/openenclave/bin/oegdb --args ...
```

Handle SIGILL:
```sh
(gdb) handle SIGILL nostop
```
