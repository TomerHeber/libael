# libael

An asynchronous event loop library written in C++, inspired by the C library [libevent](https://libevent.org).

## Install
```shell
git clone https://github.com/TomerHeber/libael
cd libael && mkdir build && cd build
cmake ..
```

##### Compile
```shell
cmake --build .
```

##### Compile & Install
```shell
cmake --build . --target install
```

## Platforms
* Linux

> Additional platforms may be added in the future.

## Features

* Simple and Modern (C++14).
* Execute a function in the context of an event loop thread.
* Event driven stream (TCP) listener.
* Event driven stream (TCP) buffer.
* Filter support for stream buffers (libael OpenSSL stream buffer is available at [libael-openssl](https://github.com/TomerHeber/libael-openssl)).

> Additional features may be added in the future (please open feature requests).
