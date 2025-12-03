#include <mpi.h>
#include <vector>
#include <iostream>
#include <algorithm>

enum Tags {
    TAG_TASK_SORT = 1,
    TAG_TASK_MERGE,
    TAG_RESULT,
    TAG_STOP
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
    int size = data.size();
    MPI_Send(&size, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
    MPI_Send(data.data(), size, MPI_INT, dest, tag, MPI_COMM_WORLD);
}

std::vector<int> recv_vector(int src, int tag) {
    MPI_Status status;
    int size;
    MPI_Recv(&size, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
    std::vector<int> data(size);
    MPI_Recv(data.data(), size, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
    return data;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        // MASTER
        std::vector<int> data = {9, 4, 7, 3, 2, 8, 5, 1, 6, 0};
        int n = data.size();
        int num_workers = size - 1;
        int chunk_size = (n + num_workers - 1) / num_workers;

        // Этап 1: Распределяем задачи сортировки
        int offset = 0;
        for (int i = 1; i <= num_workers && offset < n; ++i) {
            int end = std::min(offset + chunk_size, n);
            std::vector<int> part(data.begin() + offset, data.begin() + end);
            send_vector(i, TAG_TASK_SORT, part);
            offset = end;
        }

        // Этап 2: Сбор результатов сортировки
        std::vector<std::vector<int>> sorted_parts;
        for (int i = 1; i <= num_workers && i <= (int)data.size(); ++i) {
            auto sorted_chunk = recv_vector(i, TAG_RESULT);
            sorted_parts.push_back(std::move(sorted_chunk));
        }

        // Этап 3: Слияние по уровням
        while (sorted_parts.size() > 1) {
            std::vector<std::vector<int>> new_level;
            int i = 0;
            for (; i + 1 < (int)sorted_parts.size(); i += 2) {
                int worker = 1 + (i / 2) % num_workers;
                // Отправляем две части для слияния
                send_vector(worker, TAG_TASK_MERGE, sorted_parts[i]);
                send_vector(worker, TAG_TASK_MERGE, sorted_parts[i + 1]);
                auto merged = recv_vector(worker, TAG_RESULT);
                new_level.push_back(std::move(merged));
            }
            if (i < (int)sorted_parts.size())
                new_level.push_back(sorted_parts[i]);
            sorted_parts.swap(new_level);
        }

        // Этап 4: Завершение
        for (int i = 1; i <= num_workers; ++i) {
            MPI_Send(nullptr, 0, MPI_INT, i, TAG_STOP, MPI_COMM_WORLD);
        }

        // Вывод результата 
        for (int x : sorted_parts[0])
            std::cout << x << ' ';
        std::cout << std::endl;

    } else {
        // WORKER
        MPI_Status status;
        while (true) {
            MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == TAG_STOP)
                break;

            if (status.MPI_TAG == TAG_TASK_SORT) {
                auto vec = recv_vector(0, TAG_TASK_SORT);
                mergeSort(vec);
                send_vector(0, TAG_RESULT, vec);
            }
            else if (status.MPI_TAG == TAG_TASK_MERGE) {
                auto left = recv_vector(0, TAG_TASK_MERGE);
                auto right = recv_vector(0, TAG_TASK_MERGE);
                std::vector<int> merged(left.size() + right.size());
                std::merge(left.begin(), left.end(),
                           right.begin(), right.end(),
                           merged.begin());
                send_vector(0, TAG_RESULT, merged);
            }
        }
    }

    MPI_Finalize();
    return 0;
}
