#include <iostream>
#include <vector>
#include <thread>
#include <algorithm>
#include <future>
#include <random>
#include <chrono>

void merge(std::vector<int>& arr, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;
    
    std::vector<int> L(n1), R(n2);
    
    for (int i = 0; i < n1; i++)
        L[i] = arr[left + i];
    for (int j = 0; j < n2; j++)
        R[j] = arr[mid + 1 + j];
    
    int i = 0, j = 0;
    int k = left;
    
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }
    
    while (i < n1) {
        arr[k] = L[i];
        i++;
        k++;
    }
    
    while (j < n2) {
        arr[k] = R[j];
        j++;
        k++;
    }
}

void sequentialMergeSort(std::vector<int>& arr, int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        sequentialMergeSort(arr, left, mid);
        sequentialMergeSort(arr, mid + 1, right);
        merge(arr, left, mid, right);
    }
}

void parallelMergeSort(std::vector<int>& arr, int left, int right, int depth = 0) {
    const int THRESHOLD = 10000;
    const int MAX_DEPTH = 4;   
    
    if (left >= right) return;
    
    if (right - left < THRESHOLD || depth >= MAX_DEPTH) {
        sequentialMergeSort(arr, left, right);
        return;
    }
    
    int mid = left + (right - left) / 2;
    
    std::thread leftThread(parallelMergeSort, std::ref(arr), left, mid, depth + 1);
    std::thread rightThread(parallelMergeSort, std::ref(arr), mid + 1, right, depth + 1);
    
    leftThread.join();
    rightThread.join();
    
    merge(arr, left, mid, right);
}

int main()
{

    std::vector<int> data(2'000'000);
    for(size_t i = 0; i < data.size(); ++i) {
        data[i] = rand() % 10;
    }
    std::vector<int> data_seq = data;

    auto start_time = std::chrono::high_resolution_clock::now();
    parallelMergeSort(data, 0, data.size() - 1);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration<double>(end_time - start_time);

    std::cout << "Time Parall: " << duration.count() << "\n";

    auto start_seq = std::chrono::high_resolution_clock::now();
    sequentialMergeSort(data_seq, 0, data.size() - 1);
    auto end_seq = std::chrono::high_resolution_clock::now();
    auto duration_seq = std::chrono::duration<double>(end_seq - start_seq);

    std::cout << "Time Seq: " << duration_seq.count() << "\n";
}