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