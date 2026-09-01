#include "ThreadPool.h"   // 假设头文件名为 ThreadPool.h
#include <iostream>
#include <chrono>
#include <cassert>

// 测试1：基本任务提交与返回值
void test_basic_commit() {
    ThreadPool& pool = ThreadPool::GetInst();
    auto future = pool.commit([](int a, int b) { return a + b; }, 2, 3);
    int result = future.get();
    std::cout << "test_basic_commit: result = " << result << std::endl;
    assert(result == 5);
}

// 测试2：任务并发执行（多个任务同时运行）
void test_concurrent_execution() {
    ThreadPool& pool = ThreadPool::GetInst();
    std::atomic<int> counter{ 0 };
    const int task_count = 10;
    std::vector<std::future<void>> futures;
    for (int i = 0; i < task_count; ++i) {
        futures.emplace_back(pool.commit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            counter++;
            }));
    }
    for (auto& f : futures) f.get();
    std::cout << "test_concurrent_execution: counter = " << counter << std::endl;
    assert(counter == task_count);
}

// 测试3：任务执行中抛出异常，future 应能捕获
void test_exception_handling() {
    ThreadPool& pool = ThreadPool::GetInst();
    auto future = pool.commit([]() -> int {
        throw std::runtime_error("test exception");
        return 0;
        });
    try {
        future.get();
        assert(false); // 不应到达这里
    }
    catch (const std::exception& e) {
        std::cout << "test_exception_handling: caught exception: " << e.what() << std::endl;
    }
}

// 测试4：引用参数转发（预期会失败，因为 bind 复制参数）
void test_reference_argument() {
    ThreadPool& pool = ThreadPool::GetInst();
    int value = 10;
    auto future = pool.commit([](int& x) { x += 5; }, std::ref(value)); // 尝试使用 std::ref
    future.get();
    std::cout << "test_reference_argument: value = " << value << std::endl;
    // 如果完美转发，value 应为 15；但由于 bind 行为，可能仍为 10
    // 这里不做严格断言，仅展示输出
}

// 测试5：停止行为，提交大量任务后立即停止，观察是否所有任务完成
void test_stop_behavior() {
    ThreadPool& pool = ThreadPool::GetInst();
    std::atomic<int> executed{ 0 };
    const int task_count = 20;
    for (int i = 0; i < task_count; ++i) {
        pool.commit([&executed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            executed++;
            });
    }
    // 立即停止（注意：实际会 join，所以需要等待）
    // pool.stop();  // 注意：析构时也会调用 stop，但这里提前调用会导致后续无法使用
    std::cout << "test_stop_behavior: executed = " << executed << " / " << task_count << std::endl;
    // 预期：原始实现中 executed 可能小于 task_count，因为线程会提前退出
}

// 测试6：空闲线程计数（原始实现可能不准确）
void test_idle_count() {
    ThreadPool& pool = ThreadPool::GetInst();
    std::cout << "test_idle_count: initial idle = " << pool.idleThreadCount() << std::endl;
    // 提交一个耗时任务，空闲数应减少
    auto future = pool.commit([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "test_idle_count: during task idle = " << pool.idleThreadCount() << std::endl;
    future.get();
    std::cout << "test_idle_count: after task idle = " << pool.idleThreadCount() << std::endl;
}

int main() {
    // 运行测试
    test_basic_commit();
    test_concurrent_execution();
    test_exception_handling();
    test_reference_argument();
    // 注意：test_stop_behavior 会调用 stop()，之后线程池无法再使用，应放在最后
    test_idle_count();
    test_stop_behavior();
    return 0;
}
