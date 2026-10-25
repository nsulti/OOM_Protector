#include <iostream>
#include <chrono>
#include <thread>

int main() {
    while (true) {
        // Allocate ~1MB of RAM per iteration to cause an intentional leak
        volatile int* leak = new int[250000]();

        // Add a micro-sleep so your monitor loop actually has time to catch it 
        // before it instantly crashes your system out of memory
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}