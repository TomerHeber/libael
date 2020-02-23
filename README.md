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
#### Create a "pingpong" Server with a Stream Listener 
The server will accept connections and create a stream buffer for each new connection. Data is read from the stream buffer. If the data contains the word "ping" it will write to the buffer "pong". In this example a telnet client will be used.
###### Code
```c++
#include <iostream>
#include <chrono>
#include <memory>
#include <unordered_set>
#include <future>

#include <ael/event_loop.h>
#include <ael/stream_buffer.h>
#include <ael/stream_listener.h>

#include "elapsed.h"

using namespace std;
using namespace ael;

static Elapsed elapsed;

class PingServer : public NewConnectionHandler, public StreamBufferHandler, public enable_shared_from_this<StreamBufferHandler> {
public:
    PingServer(shared_ptr<EventLoop> event_loop) : event_loop_(event_loop) {}
    virtual ~PingServer() {}

    void HandleNewConnection(Handle handle) override {
        cout << elapsed << "new connection" << endl;
        auto stream_buffer = StreamBuffer::CreateForServer(shared_from_this(), handle);
        streams_buffers_.insert(stream_buffer);
        event_loop_->Attach(stream_buffer);
    }

    void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const std::shared_ptr<const DataView> &data_view) override {
        string data;
        data_view->AppendToString(data);

        if (data.find("ping") != string::npos) {
            stream_buffer->Write(string("pong"));
        }
    }

    void HandleConnected(std::shared_ptr<StreamBuffer>) override {}

    void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
        cout << elapsed << "connection closed" << endl;
        streams_buffers_.erase(stream_buffer);
    }
private:
    shared_ptr<EventLoop> event_loop_;
    unordered_set<std::shared_ptr<StreamBuffer>> streams_buffers_;
};

int main() 
{
    cout << elapsed << "ping server started" << endl;
    auto event_loop = EventLoop::Create();
    auto ping_server = make_shared<PingServer>(event_loop);
    auto stream_listener = StreamListener::Create(ping_server, "127.0.0.1", 12345);
    event_loop->Attach(stream_listener);

    promise<void>().get_future().wait();
}
```
###### Output + Telnet Input
```shell
 ./ping_server &
[1] 6160
[thread #140371459032896][   0ms] ping server started
telnet 127.0.0.1 12345
Trying 127.0.0.1...
[thread #140371438397184][5838ms] new connection
Connected to 127.0.0.1.
Escape character is '^]'.
ping
pongping
pongping
pongping
pong
pong^]
telnet> quit
[thread #140371438397184][53636ms] connection closed
```
Instead of using Telnet a stream buffer may be used as a "client" to communicate with the server.
###### Code
```c++
#include <iostream>
#include <chrono>
#include <memory>
#include <future>

#include <ael/event_loop.h>
#include <ael/stream_buffer.h>

#include "elapsed.h"

using namespace std;
using namespace ael;

static Elapsed elapsed;

class PingClient : public StreamBufferHandler {
public:
    PingClient(shared_ptr<promise<void>> done_promise) : done_promise_(done_promise) {}
    virtual ~PingClient() {}

    void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const std::shared_ptr<const DataView> &data_view) override {
        string data;
        data_view->AppendToString(data);

        if (data.find("pong") != string::npos) {
            cout << elapsed << "pong received, closing connection" << endl;
            stream_buffer->Close();
        }
    }

    void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override { 
        // After connected, write "ping" to server.
        cout << elapsed << "connected, sending ping" << endl;
        stream_buffer->Write(string("ping"));
    }

    void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
        cout << elapsed << "connection closed" << endl;
        // Release the "main" thread.
        done_promise_->set_value();
    }
private:
    shared_ptr<promise<void>> done_promise_;
};

int main() 
{
    cout << elapsed << "ping client started" << endl;
    
    auto event_loop = EventLoop::Create();
    auto done_promise = make_shared<promise<void>>();
    shared_future<void> done_future(done_promise->get_future());
    auto ping_client = make_shared<PingClient>(done_promise);
    auto stream_buffer = StreamBuffer::CreateForClient(ping_client, "127.0.0.1", 12345);
    event_loop->Attach(stream_buffer);

    done_future.wait();
    EventLoop::DestroyAll();

    cout << elapsed << "ping client finished" << endl;
}
```
###### Server Output
```shell
[thread #140150337836864][   0ms] ping server started
[thread #140150317201152][2295ms] new connection
[thread #140150317201152][2295ms] connection closed
```
###### Client Output
```shell
./ping_client
[thread #140573192685376][   0ms] ping client started
[thread #140573172049664][   0ms] connected, sending ping
[thread #140573172049664][   0ms] pong received, closing connection
[thread #140573172049664][   0ms] connection closed
[thread #140573192685376][   1ms] ping client finished
```
