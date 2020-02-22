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
    auto event_loop = EventLoop::Create();
    event_loop->ExecuteOnce(&Printer::Print1, print_class, 100);
    event_loop->ExecuteOnce(&Printer::Print2, print_class, 2000, "hello");    
    this_thread::sleep_for(100ms);
    EventLoop::DestroyAll();
    cout << elapsed << "sample ended" << endl;
}