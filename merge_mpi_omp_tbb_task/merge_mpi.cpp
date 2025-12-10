#include <mpi.h>
#include <vector>
#include <queue>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cstdlib>
#include <cmath> 

enum Tag {
    TAG_TASK_SORT = 1,
    TAG_TASK_MERGE,
    TAG_RESULT,
    TAG_STOP
};

struct Task {
    int type;
    std::vector<int> data1;
    std::vector<int> data2;
    int priority;
};

struct TaskCompare {
    bool operator()(const Task& a, const Task& b) const {
        return a.priority > b.priority;
    }
};

void mergeSort(std::vector<int>& arr) {
    if (arr.size() <= 1) return;
    size_t mid = arr.size() / 2;
    std::vector<int> left(arr.begin(), arr.begin() + mid);
    std::vector<int> right(arr.begin() + mid, arr.end());
    mergeSort(left);
    mergeSort(right);
    std::merge(left.begin(), left.end(), right.begin(), right.end(), arr.begin());
}

void send_vector(int dest, int tag, const std::vector<int>& data) {
    int size = (int)data.size();
    MPI_Send(&size, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
    if (size > 0)
        MPI_Send(data.data(), size, MPI_INT, dest, tag, MPI_COMM_WORLD);
}

std::vector<int> recv_vector(int src, int tag) {
    MPI_Status status;
    int size;
    MPI_Recv(&size, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
    std::vector<int> data(size);
    if (size > 0)
        MPI_Recv(data.data(), size, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
    return data;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int N = 2'000'000;
    double t_parallel = 0.0; 
    std::vector<std::vector<int>> results;

    if (rank == 0) {
        // MASTER 
        std::vector<int> data(N);
        for (int i = 0; i < N; ++i) data[i] = rand() % N;

        // Измеряем std::sort 
        std::vector<int> data_std = data;
        double t_start_std = MPI_Wtime();
        std::sort(data_std.begin(), data_std.end());
        double t_end_std = MPI_Wtime();
        double t_std = t_end_std - t_start_std;
 
        double t_start_parallel = MPI_Wtime();

        int num_workers = size - 1;

        if (num_workers > 0) {
            // --- РЕЖИМ: Параллельное выполнение с воркерами (size > 1) ---
            std::priority_queue<Task, std::vector<Task>, TaskCompare> task_queue;

            // Этап 1: создаём задачи сортировки
            int chunk_size = (data.size() + num_workers - 1) / num_workers;
            for (int i = 0; i < (int)data.size(); i += chunk_size) {
                Task t;
                t.type = TAG_TASK_SORT;
                t.priority = 0; 
                t.data1 = std::vector<int>(data.begin() + i,
                                           data.begin() + std::min<int>(i + chunk_size, data.size()));
                task_queue.push(t);
            }

            int active_workers = 0;

            while (!task_queue.empty() || active_workers > 0) {
                MPI_Status status;

                // Назначаем задачи свободным воркерам
                for (int w = 1; w <= num_workers && !task_queue.empty(); ++w) {
                    int flag;
                    MPI_Iprobe(w, TAG_RESULT, MPI_COMM_WORLD, &flag, &status);
                    if (flag == 0) { // воркер свободен
                        Task t = task_queue.top();
                        task_queue.pop();

                        if (t.type == TAG_TASK_SORT) {
                            send_vector(w, TAG_TASK_SORT, t.data1);
                        } else {
                            send_vector(w, TAG_TASK_MERGE, t.data1);
                            send_vector(w, TAG_TASK_MERGE, t.data2);
                        }
                        active_workers++;
                    }
                }

                // Готовые результаты
                int flag;
                MPI_Iprobe(MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &flag, &status);
                if (flag) {
                    int src = status.MPI_SOURCE;
                    auto result = recv_vector(src, TAG_RESULT);
                    results.push_back(result);
                    active_workers--;

                    // Создаём новые задачи merge
                    if (task_queue.empty() && active_workers == 0 && results.size() > 1) {
                        std::vector<std::vector<int>> new_level;
                        for (size_t i = 0; i + 1 < results.size(); i += 2) {
                            Task merge_task;
                            merge_task.type = TAG_TASK_MERGE;
                            merge_task.priority = 1; 
                            merge_task.data1 = results[i];
                            merge_task.data2 = results[i + 1];
                            task_queue.push(merge_task);
                        }
                        if (results.size() % 2 == 1)
                            new_level.push_back(results.back());
                        results.swap(new_level);
                    }
                }
            }
            
            // Отправляем сигнал остановки всем воркерам
            for (int w = 1; w <= num_workers; ++w)
                MPI_Send(nullptr, 0, MPI_INT, w, TAG_STOP, MPI_COMM_WORLD);

        } else {
            // --- РЕЖИМ: Одиночный процесс (size == 1) ---
            results.push_back(data); 
            mergeSort(results[0]);
        }

        t_parallel = MPI_Wtime() - t_start_parallel;


        // Печать результатов
        std::cout << "\n=== РЕЗУЛЬТАТЫ ===\n";
        std::cout << "Размер массива: " << N << "\n";
        std::cout << "MPI процессов:  " << size << "\n";
        std::cout << "std::sort:      " << t_std << " сек\n";
        std::cout << "Параллельно:    " << t_parallel << " сек\n\n";

        if (!results.empty() && results[0].size() == N) {
            bool ok = (results[0] == data_std);
            std::cout << (ok ? "Результат совпадает с std::sort\n"
                              : "Ошибка в результате сортировки\n");
        } else if (size > 1) {
             std::cout << "Ошибка: Результат параллельной сортировки не был получен.\n";
        }


    } else {
        // WORKERS (Ранги > 0)
        MPI_Status status;
        while (true) {
            MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            
            if (status.MPI_TAG == TAG_STOP) {
                int dummy;
                MPI_Recv(&dummy, 0, MPI_INT, 0, TAG_STOP, MPI_COMM_WORLD, &status);
                break;
            }

            if (status.MPI_TAG == TAG_TASK_SORT) {
                auto vec = recv_vector(0, TAG_TASK_SORT);
                mergeSort(vec);
                send_vector(0, TAG_RESULT, vec);
            }
            else if (status.MPI_TAG == TAG_TASK_MERGE) {
                auto left = recv_vector(0, TAG_TASK_MERGE);
                auto right = recv_vector(0, TAG_TASK_MERGE);
                std::vector<int> merged(left.size() + right.size());
                std::merge(left.begin(), left.end(), right.begin(), right.end(), merged.begin());
                send_vector(0, TAG_RESULT, merged);
            }
        }
    }

    MPI_Finalize();
    return 0;
}