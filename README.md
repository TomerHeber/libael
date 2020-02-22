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
* Event driven stream listener (TCP).
* Event driven stream buffer (TCP).
* Filter support for stream buffers (libael OpenSSL filter is available at [libael-openssl](https://github.com/TomerHeber/libael-openssl)).

> Additional features may be added in the future (please open feature requests).

## Usage Samples
Sample files are available in the [samples](samples/) directory
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

Methods may be executed in the future or in intervals. They may also be cancled.
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

    void Print1() {
        cout << elapsed << "Print1" << endl;
    }

    void Print2() {
        cout << elapsed << "Print2" << endl;
    }

    void Print3() {
        cout << elapsed << "Print3" << endl;
    }
};

int main() 
{
    cout << elapsed << "sample started" << endl;
    auto print_class = make_shared<Printer>();
    auto event_loop = EventLoop::Create();
    // Call Print1 every 100ms.
    auto handle = event_loop->ExecuteInterval(100ms, &Printer::Print1, print_class);
    // Call Print2 every 200ms.
    event_loop->ExecuteInterval(200ms, &Printer::Print2, print_class);   
    // Call Print3 once in 500ms.
    event_loop->ExecuteOnceIn(500ms, &Printer::Print3, print_class);
    this_thread::sleep_for(450ms);
    // Cancel/Stop "Print1 interval".
    handle->Cancel();
    cout << elapsed << "Print1 interval canceled" << endl;
    this_thread::sleep_for(450ms);
    EventLoop::DestroyAll();
    cout << elapsed << "sample ended" << endl;
}
```
###### Output
```shell
[thread #140037509097280][   0ms] sample started
[thread #140037488461568][   0ms] Print1
[thread #140037488461568][   0ms] Print2
[thread #140037488461568][ 100ms] Print1
[thread #140037488461568][ 200ms] Print1
[thread #140037488461568][ 200ms] Print2
[thread #140037488461568][ 300ms] Print1
[thread #140037488461568][ 403ms] Print1
[thread #140037488461568][ 403ms] Print2
[thread #140037509097280][ 451ms] Print1 interval canceled
[thread #140037488461568][ 505ms] Print3
[thread #140037488461568][ 600ms] Print2
[thread #140037488461568][ 802ms] Print2
[thread #140037509097280][ 902ms] sample ended
```
