# libael

An asynchronous event loop library written in C++ and inspired by the C library [libevent](https://libevent.org).

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

> Additional platforms may be added in the future (please open feature requests).

## Features

* Simple and Modern (C++14).
* Execute a function in the context of an event loop thread.
* Event driven stream (TCP) listener.
* Event driven stream (TCP) buffer.
* Filter support for stream buffers (libael OpenSSL filter is available at [libael-openssl](https://github.com/TomerHeber/libael-openssl)).

> Additional features may be added in the future (please open feature requests).

## Usage Samples
#### Create an Event Loop and Execute Methods
###### Code
```c++
#include <iostream>
#include <chrono>
#include <memory>

#include <ael/event_loop.h>

#include "elapsed.h"

using namespace std;
using namespace ael;

static Elapsed elapsed;

class Printer {
public:
    Printer() {}
    virtual ~Printer() {}

    void Print1(int i) {
        cout << elapsed << "Print1 " << i << endl;
    }

    void Print2(int i, string j) {
        cout << elapsed << "Print2 " << i << " " << j << endl;
    }
};

int main() 
{
    cout << elapsed << "sample started" << endl;
    auto print_class = make_shared<Printer>();
    // Create an event loop.
    auto event_loop = EventLoop::Create();
    // Execute "now" in the context of the event_loop thread.
    event_loop->ExecuteOnce(&Printer::Print1, print_class, 100);
    // Execute "now" in the context of the event_loop thread.
    event_loop->ExecuteOnce(&Printer::Print2, print_class, 2000, "hello");
    this_thread::sleep_for(100ms);
    EventLoop::DestroyAll();
    cout << elapsed << "sample ended" << endl;
}
```
###### Output
```shell
[thread #139739746940736][   0ms] sample started
[thread #139739726305024][   0ms] Print1 100
[thread #139739726305024][   0ms] Print2 2000 hello
[thread #139739746940736][ 101ms] sample ended
```
