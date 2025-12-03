#include <tbb/tbb.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdlib>

void mergeSort(std::vector<int>& arr) {
    if (arr.size() <= 1) return;
    size_t mid = arr.size() / 2;
    std::vector<int> left(arr.begin(), arr.begin() + mid);
    std::vector<int> right(arr.begin() + mid, arr.end());
    mergeSort(left);
    mergeSort(right);
    std::merge(left.begin(), left.end(), right.begin(), right.end(), arr.begin());
}

int main() {
    const int N = 2'000'000; 
    std::vector<int> data(N);

    for(int i = 0; i < N; i++)
        data[i] = rand() % N;

    // std::sort
    std::vector<int> data_std = data;
    tbb::tick_count t0 = tbb::tick_count::now();
    std::sort(data_std.begin(), data_std.end());
    tbb::tick_count t1 = tbb::tick_count::now();
    double T_std = (t1 - t0).seconds();
    std::cout << "std::sort: " << T_std << " сек\n\n";


    for(int threads = 1; threads <= 6; threads++){
        tbb::global_control gc(tbb::global_control::max_allowed_parallelism, threads);

        std::vector<int> data_parallel = data;
        int grains = threads;
        int chunk = (N + grains - 1) / grains;
        std::vector<std::vector<int>> parts(grains);

        tbb::tick_count t_start = tbb::tick_count::now();

        tbb::parallel_for(0, grains, [&](int i){
            int l = i * chunk;
            int r = std::min(N, l + chunk);
            if(l >= N) return;
            parts[i] = std::vector<int>(data_parallel.begin() + l, data_parallel.begin() + r);
            mergeSort(parts[i]);
        });


        int active = grains;
        while(active > 1){
            int new_active = (active + 1) / 2;
            std::vector<std::vector<int>> new_parts(new_active);

            tbb::parallel_for(0, active/2, [&](int i){
                int a = 2*i;
                int b = a+1;
                std::vector<int> merged(parts[a].size() + parts[b].size());
                std::merge(parts[a].begin(), parts[a].end(),
                           parts[b].begin(), parts[b].end(), merged.begin());
                new_parts[i] = std::move(merged);
            });

            if(active % 2 == 1)
                new_parts.back() = std::move(parts.back());

            parts.swap(new_parts);
            active = new_active;
        }

        tbb::tick_count t_end = tbb::tick_count::now();
        double T_parallel = (t_end - t_start).seconds();

        std::cout << "TBB merge-sort, потоки = " << threads 
                  << ": " << T_parallel 
                  << " сек, "
                  << (parts[0] == data_std ? "✓ корректно" : "✗ ошибка") 
                  << "\n";
    }

    return 0;
}
