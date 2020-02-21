#include <iostream>
#include <chrono>
#include <memory>

#include <ael/event_loop.h>

using namespace std;
using namespace ael;

class Printer {
public:
    Printer() {}
    virtual ~Printer() {}

    void Print1(int i) {
        cout << "Print1 " << i << endl;
    }

    void Print2(int i, string j) {
        cout << "Print2 " << i << " " << j << endl;
    }
};

int main() 
{
    cout << "sample started" << endl;
    auto print_class = make_shared<Printer>();
    auto event_loop = EventLoop::Create();
    event_loop->ExecuteOnce(&Printer::Print1, print_class, 100);
    event_loop->ExecuteOnce(&Printer::Print2, print_class, 100, "hello");
    this_thread::sleep_for(100ms);
    EventLoop::DestroyAll();
    cout << "sample ended" << endl;
}