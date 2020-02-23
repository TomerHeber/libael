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