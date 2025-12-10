#include <iostream>
#include <vector>
#include <chrono>
#include <omp.h>
#include <cstdint>

int64_t sum_parallel(const std::vector<int>& a) {
    int64_t s = 0;

#pragma omp parallel for reduction(+:s)
    for (size_t i = 0; i < a.size(); i++) {
        s += a[i];
    }
    return s;
}

int main() {
    size_t n = 200'000'000;
    std::vector<int> a(n, 1);

    auto t1 = std::chrono::high_resolution_clock::now();
    int64_t s = sum_parallel(a);
    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> dt = t2 - t1;
    std::cout << "Sum = " << s << "\n";
    std::cout << "Time: " << dt.count() << " sec\n";
}
