#include <tbb/tbb.h>
#include <vector>
#include <algorithm>
#include <iostream>

// Последовательная сортировка по индексам (merge + copy-back)
void mergeSortSequential(std::vector<int>& a, std::vector<int>& tmp, int l, int r) {
    if (r - l <= 1) return;

    int m = (l + r) / 2;

    mergeSortSequential(a, tmp, l, m);
    mergeSortSequential(a, tmp, m, r);

    std::merge(a.begin() + l, a.begin() + m,
               a.begin() + m, a.begin() + r,
               tmp.begin() + l);

    std::copy(tmp.begin() + l, tmp.begin() + r, a.begin() + l);
}

// Параллельная сортировка на TBB tasks
void mergeSortTBB(std::vector<int>& a, std::vector<int>& tmp,
                  int l, int r, int depth)
{
    if (r - l <= 1)
        return;

    const int THRESHOLD = 50000;

    if (depth <= 0 || (r - l) < THRESHOLD) {
        mergeSortSequential(a, tmp, l, r);
        return;
    }

    int m = (l + r) / 2;

    tbb::task_group tg;

    tg.run([&]{ mergeSortTBB(a, tmp, l, m, depth - 1); });
    tg.run([&]{ mergeSortTBB(a, tmp, m, r, depth - 1); });

    tg.wait();

    // merge into tmp, then copy back
    std::merge(a.begin() + l, a.begin() + m,
               a.begin() + m, a.begin() + r,
               tmp.begin() + l);

    std::copy(tmp.begin() + l, tmp.begin() + r, a.begin() + l);
}

int main() {
    const int N = 2'000'000;

    std::vector<int> data(N);
    for (int i = 0; i < N; i++)
        data[i] = rand() % N;

    // std::sort
    std::vector<int> data_std = data;
    auto t0 = tbb::tick_count::now();
    std::sort(data_std.begin(), data_std.end());
    auto t1 = tbb::tick_count::now();

    std::cout << "std::sort: " << (t1 - t0).seconds() << " сек\n\n";

    for (int threads = 1; threads <= 6; threads++) {

        tbb::global_control gc(tbb::global_control::max_allowed_parallelism, threads);

        std::vector<int> data_par = data;
        std::vector<int> tmp(N);

        int MAX_DEPTH = std::log2(threads) + 2;   // оптимально

        auto ts = tbb::tick_count::now();

        mergeSortTBB(data_par, tmp, 0, N, MAX_DEPTH);

        auto te = tbb::tick_count::now();

        std::cout << "TBB tasks, threads = " << threads
                  << ": " << (te - ts).seconds() << " сек,  "
                  << (data_par == data_std ? "✓ корректно" : "✗ ошибка")
                  << "\n";
    }

    return 0;
}
