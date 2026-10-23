#include <unistd.h>

int main() {
  while (true) {
    volatile int * leak = new int[250000](); // intentional memory leak to test program
  }
}
