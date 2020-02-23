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

	void HandleConnected(std::shared_ptr<StreamBuffer>) override { /* required for filters (check libael_openssl) */ }

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