#include <omp.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <numeric>
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
    const int threads = omp_get_max_threads();

    std::vector<int> data(N);
    for(int i=0;i<N;i++) data[i] = rand() % N;

    // std::sort
    std::vector<int> data_std = data;
    double t0 = omp_get_wtime();
    std::sort(data_std.begin(), data_std.end());
    double t1 = omp_get_wtime();
    double T_std = t1 - t0;


    double t2 = omp_get_wtime();

    int chunk = (N + threads - 1) / threads;
    std::vector<std::vector<int>> parts(threads);

    // Параллельная сортировка
    #pragma omp parallel for
    for(int i=0;i<threads;i++){
        int l = i*chunk;
        int r = std::min(N, l+chunk);
        if (l>=N) continue;
        parts[i] = std::vector<int>(data.begin()+l,data.begin()+r);
        mergeSort(parts[i]);
    }

    // Cлияние
    int active = threads;
    std::vector<std::vector<int>> next_parts(threads);

    while(active > 1){
        int new_active = (active + 1) / 2;

        #pragma omp parallel for
        for (int i = 0; i < new_active; i++) {
            int a = 2*i;
            int b = a + 1;

            if (b >= active) {
                next_parts[i] = parts[a];
                continue;
            }

            next_parts[i].resize(parts[a].size() + parts[b].size());
            std::merge(
                parts[a].begin(), parts[a].end(),
                parts[b].begin(), parts[b].end(),
                next_parts[i].begin()
            );
        }

        parts.swap(next_parts);

        active = new_active;
    }


    std::vector<int>& result = parts[0];
    double t3 = omp_get_wtime();
    double T_parallel = t3 - t2;

    std::cout << "\n=== РЕЗУЛЬТАТЫ ===\n";
    std::cout << "Размер массива: " << N << "\n";
    std::cout << "Потоков:        " << threads << "\n";
    std::cout << "std::sort:      " << T_std      << " сек\n";
    std::cout << "OpenMP merge:   " << T_parallel << " сек\n";

    std::cout << (result == data_std
                    ? "✓ Результат корректный\n"
                    : "✗ Ошибка сортировки\n");

    return 0;
}
