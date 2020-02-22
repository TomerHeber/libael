#ifndef INCLUDE_ELAPSED_H_
#define INCLUDE_ELAPSED_H_

#include <chrono>
#include <iostream>
#include <iomanip>

class Elapsed {
public:
    Elapsed() {
        start_ = std::chrono::steady_clock::now();
    }
    virtual ~Elapsed() {}

    friend std::ostream& operator<<(std::ostream &out, const Elapsed &elapsed) {
        auto now = std::chrono::steady_clock::now();
        auto ela = std::chrono::duration_cast<std::chrono::milliseconds>(now - elapsed.start_).count();
        out << "[thread #" << std::this_thread::get_id() << "][" << std::setfill(' ') << std::setw(4) << ela << "ms] ";
        return out;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> start_;
};

#endif /* INCLUDE_ELAPSED_H_ */