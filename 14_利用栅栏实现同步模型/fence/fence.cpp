#include <iostream>
#include <atomic>
#include <thread>
#include <cassert>
std::atomic<bool> x, y;
std::atomic<int> z;

void write_x()
{
    x.store(true, std::memory_order_release); //1
}
void write_y()
{
    y.store(true, std::memory_order_release); //2
}
void read_x_then_y()
{
    while (!x.load(std::memory_order_acquire));
    if (y.load(std::memory_order_acquire))   //3
        ++z;
}
void read_y_then_x()
{
    while (!y.load(std::memory_order_acquire));
    if (x.load(std::memory_order_acquire))   //4
        ++z;
}

void TestAR()
{
    x = false;
    y = false;
    z = 0;
    std::thread a(write_x);
    std::thread b(write_y);
    std::thread c(read_x_then_y);
    std::thread d(read_y_then_x);
    a.join();
    b.join();
    c.join();
    d.join();
    assert(z.load() != 0); //5
    std::cout << "z value is " << z.load() << std::endl;
}



void write_x_then_y2()
{
    x.store(true, std::memory_order_relaxed); // 1
    y.store(true, std::memory_order_relaxed);   // 2
}
void read_y_then_x2()
{
    while (!y.load(std::memory_order_relaxed));  // 3
    if (x.load(std::memory_order_relaxed))  // 4
        ++z;
}

void TestRelaxed()
{
    x = false;
    y = false;
    z = 0;
    std::thread a(write_x_then_y2);
    std::thread b(read_y_then_x2);
    a.join();
    b.join();
    assert(z.load() != 0);  //5
}


void write_x_then_y3()
{
    x.store(true, std::memory_order_relaxed); // 1
    y.store(true, std::memory_order_release);   // 2
}

void read_y_then_x3()
{
    while (!y.load(std::memory_order_acquire));  // 3
    if (x.load(std::memory_order_relaxed))  // 4
        ++z;
}

void write_x_then_y_fence()
{
    x.store(true, std::memory_order_relaxed);  //1
    std::atomic_thread_fence(std::memory_order_release);  //2
    y.store(true, std::memory_order_relaxed);  //3
}

void read_y_then_x_fence()
{
    while (!y.load(std::memory_order_relaxed));  //4
    std::atomic_thread_fence(std::memory_order_acquire); //5
    if (x.load(std::memory_order_relaxed))  //6
        ++z;
}

void TestFence()
{
    x = false;
    y = false;
    z = 0;
    std::thread a(write_x_then_y_fence);
    std::thread b(read_y_then_x_fence);
    a.join();
    b.join();
    assert(z.load() != 0);   //7
}


int main()
{
    //TestAR();
    //TestRelaxed();
    TestFence();
    std::cout << "Main Exited" << std::endl;
}

