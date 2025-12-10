#include <omp.h>
#include <vector>
#include <iostream>
#include <algorithm>

// Последовательная сортировка
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

// Параллельная сортировка с задачами OpenMP
void mergeSortParallel(std::vector<int>& a, std::vector<int>& tmp,
                       int l, int r, int depth)
{
    if (r - l <= 1)
        return;

    // Порог для последовательной сортировки
    const int THRESHOLD = 50000;

    if (depth <= 0 || (r - l) < THRESHOLD) {
        mergeSortSequential(a, tmp, l, r);
        return;
    }

    int m = (l + r) / 2;

    // Левая задача
    #pragma omp task shared(a, tmp) if (depth > 0)
    {
        mergeSortParallel(a, tmp, l, m, depth - 1);
    }

    // Правая задача
    #pragma omp task shared(a, tmp) if (depth > 0)
    {
        mergeSortParallel(a, tmp, m, r, depth - 1);
    }

    #pragma omp taskwait

    // Merge в tmp, затем copy back
    std::merge(a.begin() + l, a.begin() + m,
               a.begin() + m, a.begin() + r,
               tmp.begin() + l);

    std::copy(tmp.begin() + l, tmp.begin() + r, a.begin() + l);
}

int main() {
    const int N = 2'000'000;
    const int threads = omp_get_max_threads();
    const int MAX_DEPTH = 4;  // log2(потоки)

    std::vector<int> data(N);
    for (int i = 0; i < N; i++)
        data[i] = rand() % N;

    // std::sort
    std::vector<int> data_std = data;
    double t0 = omp_get_wtime();
    std::sort(data_std.begin(), data_std.end());
    double t1 = omp_get_wtime();
    double result_time_sort = (t1 - t0);

    // parallel merge sort
    std::vector<int> data_par = data;
    std::vector<int> tmp(N);

    double t2 = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp single
        {
            mergeSortParallel(data_par, tmp, 0, N, MAX_DEPTH);
        }
    }
    double t3 = omp_get_wtime();

    std::cout << "Размер массива: " << N << "\n";
    std::cout << "Потоков:        " << threads << "\n";
    std::cout << "std::sort:     " << result_time_sort << " sec\n";
    std::cout << "OpenMP merge:  " << (t3 - t2) << " sec\n";


    std::cout << (data_par == data_std
                      ? "✓ correct\n"
                      : "✗ wrong\n");

    return 0;
}
