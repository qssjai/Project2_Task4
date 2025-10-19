// Compiler: MSVC (Visual Studio 2022), C++20, build in RELEASE mode

#include <iostream>
#include <vector>
#include <algorithm>
#include <execution>
#include <thread>
#include <numeric>
#include <random>
#include <chrono>

using namespace std;

vector<int> generate_random_vector(size_t size, unsigned seed = 42) {

    vector<int> numbers(size);
    mt19937 rng(seed);
    uniform_int_distribution<int> dist(0, 1'000'000);
    for (auto& num : numbers) num = dist(rng);
    return numbers;
}

bool is_greater_than_500k(int x) { return x > 500'000; }
bool is_divisible_by_7(int x) { return x % 7 == 0; }

template<typename Iterator, typename Predicate>
size_t custom_parallel_count(Iterator begin, Iterator end, Predicate predicate, int num_threads) {
    size_t total_size = distance(begin, end);
    if (total_size == 0 || num_threads <= 1)
        return count_if(begin, end, predicate);

    num_threads = min<size_t>(num_threads, thread::hardware_concurrency());
    vector<thread> threads;
    vector<size_t> partial_results(num_threads, 0);

    size_t block_size = total_size / num_threads;
    Iterator block_start = begin;

    for (int i = 0; i < num_threads; ++i) {
        Iterator block_end = (i == num_threads - 1) ? end : next(block_start, block_size);
        threads.emplace_back([block_start, block_end, &predicate, &partial_results, i]() {
            partial_results[i] = count_if(block_start, block_end, predicate);
            });
        block_start = block_end;
    }

    for (auto& thread_item : threads) thread_item.join();
    return accumulate(partial_results.begin(), partial_results.end(), size_t(0));
}

template<typename Function>
long long measure_time_ms(Function func) {
    auto start = chrono::high_resolution_clock::now();
    func();
    auto end = chrono::high_resolution_clock::now();
    return chrono::duration_cast<chrono::milliseconds>(end - start).count();
}

template<typename Predicate>
void run_experiment(const vector<int>& numbers, const string& label, Predicate predicate) {
    cout << "\n=== " << label << " ===\n";

    size_t result = 0;
    long long time_no_policy = measure_time_ms([&]() { result = count_if(numbers.begin(), numbers.end(), predicate); });
    long long time_seq = measure_time_ms([&]() { result = count_if(execution::seq, numbers.begin(), numbers.end(), predicate); });
    long long time_par = measure_time_ms([&]() { result = count_if(execution::par, numbers.begin(), numbers.end(), predicate); });
    long long time_par_unseq = measure_time_ms([&]() { result = count_if(execution::par_unseq, numbers.begin(), numbers.end(), predicate); });

    cout << "No policy:        " << time_no_policy << " ms\n";
    cout << "execution::seq:      " << time_seq << " ms\n";
    cout << "execution::par:      " << time_par << " ms\n";
    cout << "execution::par_unseq:" << time_par_unseq << " ms\n";

    cout << "\nCustom parallel implementation:\n";
    int hardware_threads = thread::hardware_concurrency();
    if (hardware_threads == 0) hardware_threads = 4;

    int best_threads = 1;
    long long best_time = LLONG_MAX;

    for (int num_threads = 1; num_threads <= 2 * hardware_threads; ++num_threads) {
        long long time_current = measure_time_ms([&]() {
            result = custom_parallel_count(numbers.begin(), numbers.end(), predicate, num_threads);
            });
        cout << "Threads = " << num_threads << " -> " << time_current << " ms\n";
        if (time_current < best_time) {
            best_time = time_current;
            best_threads = num_threads;
        }
    }

    cout << "\nThe best K = " << best_threads
        << " (hardware threads: " << hardware_threads << ")\n";
}

int main() {
    cout << "C++ version: " << __cplusplus << endl;
#ifdef NDEBUG
    cout << "Build: RELEASE" << endl;
#else
    cout << "Build: DEBUG" << endl;
#endif
    cout << "Hardware threads: " << thread::hardware_concurrency() << endl;

    vector<size_t> data_sizes = { 100'000, 1'000'000, 5'000'000 };

    for (auto data_size : data_sizes) {
        auto numbers = generate_random_vector(data_size, data_size + 100);
        run_experiment(numbers, "Dataset N=" + to_string(data_size) + ", x > 500000", is_greater_than_500k);
        run_experiment(numbers, "Dataset N=" + to_string(data_size) + ", x % 7 == 0", is_divisible_by_7);
    }

    return 0;
}
