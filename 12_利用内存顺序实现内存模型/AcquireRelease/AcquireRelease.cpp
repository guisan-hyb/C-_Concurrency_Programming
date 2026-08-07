#include <iostream>
#include <atomic>
#include <thread>
#include <cassert>
#include <vector>
#include <algorithm>
#include <mutex>
#include <memory>

std::atomic<bool> x, y;
std::atomic<int> z;

void write_x_then_y() {
	x.store(true, std::memory_order_seq_cst);  // 1
	y.store(true, std::memory_order_seq_cst);  // 2
}

void read_y_then_x() {
	while (!y.load(std::memory_order_seq_cst)) { // 3
		std::cout << "y load false" << std::endl;
	}

	if (x.load(std::memory_order_seq_cst)) { //4
		++z;
	}

}

void TestOrderSeqCst() {
	std::thread t1(write_x_then_y);
	std::thread t2(read_y_then_x);
	t1.join();
	t2.join();
	assert(z.load() != 0); // 5
}

void TestOrderRelaxed() {
	std::atomic<bool> rx, ry;

	//sequenc before
	std::thread t1([&]() {
		rx.store(true, std::memory_order_relaxed); // 1
		ry.store(true, std::memory_order_relaxed); // 2
		});


	std::thread t2([&]() {
		while (!ry.load(std::memory_order_relaxed)); //3
		assert(rx.load(std::memory_order_relaxed)); //4
		});

	t1.join();
	t2.join();
}

void TestReleaseAcquire() {
	std::atomic<bool> rx, ry;

	std::thread t1([&]() {
		rx.store(true, std::memory_order_relaxed); // 1
		ry.store(true, std::memory_order_release); // 2
		});


	std::thread t2([&]() {
		//sequence -before
		while (!ry.load(std::memory_order_acquire)); //3
		assert(rx.load(std::memory_order_relaxed)); //4
		});

	t1.join();
	t2.join();
}


void ReleasAcquireDanger() {
	std::atomic<int> xd{ 0 }, yd{ 0 };
	std::atomic<int> zd;

	std::thread t1([&]() {
		xd.store(1, std::memory_order_release);  // (1)
		yd.store(1, std::memory_order_release); //  (2)
		});

	std::thread t2([&]() {
		yd.store(2, std::memory_order_release);  // (3)
		});


	std::thread t3([&]() {
		while (!yd.load(std::memory_order_acquire)); //（4）
		assert(xd.load(std::memory_order_acquire) == 1); // (5)
		});

	t1.join();
	t2.join();
	t3.join();
}

void ReleaseSequence() {
	std::vector<int> data;
	std::atomic<int> flag{ 0 };

	std::thread t1([&]() {
		data.push_back(42);  //(1)
		flag.store(1, std::memory_order_release); //(2) A
		});

	std::thread t2([&]() {
		int expected = 1;
		while (!flag.compare_exchange_strong(expected, 2, std::memory_order_relaxed)) // (3)
			expected = 1;
		});

	std::thread t3([&]() {
		while (flag.load(std::memory_order_acquire) < 2); // (4)
		assert(data.at(0) == 42); // (5)
		});

	t1.join();
	t2.join();
	t3.join();
}

void ConsumeDependency() {
	std::atomic<std::string*> ptr;
	int data;

	std::thread t1([&]() {
		std::string* p = new std::string("Hello World"); // (1)
		data = 42; // (2)
		ptr.store(p, std::memory_order_release); // (3)
		});

	std::thread t2([&]() {
		std::string* p2;
		while (!(p2 = ptr.load(std::memory_order_consume))); // (4)
		assert(*p2 == "Hello World"); // (5)
		assert(data == 42); // (6)
		});

	t1.join();
	t2.join();
}


//利用智能指针解决释放问题
class SingleAuto
{
private:
	SingleAuto()
	{
	}
	SingleAuto(const SingleAuto&) = delete;
	SingleAuto& operator=(const SingleAuto&) = delete;
public:
	~SingleAuto()
	{
		std::cout << "single auto delete success " << std::endl;
	}
	static std::shared_ptr<SingleAuto> GetInst()
	{
		// 1 处
		if (single != nullptr)
		{
			return single;
		}
		// 2 处
		s_mutex.lock();
		// 3 处
		if (single != nullptr)
		{
			s_mutex.unlock();
			return single;
		}
		// 4处
		single = std::shared_ptr<SingleAuto>(new SingleAuto);
		s_mutex.unlock();
		return single;
	}
private:
	static std::shared_ptr<SingleAuto> single;
	static std::mutex s_mutex;
};


std::shared_ptr<SingleAuto> SingleAuto::single = nullptr;
std::mutex SingleAuto::s_mutex;

void TestSingle() {
	std::thread t1([]() {
		std::cout << "thread t1 singleton address is  0x: " << SingleAuto::GetInst() << std::endl;
		});

	std::thread t2([]() {
		std::cout << "thread t2 singleton address is 0x: " << SingleAuto::GetInst() << std::endl;
		});

	t2.join();
	t1.join();
}


//利用智能指针解决释放问题
class SingleMemoryModel
{
private:
	SingleMemoryModel()
	{
	}
	SingleMemoryModel(const SingleMemoryModel&) = delete;
	SingleMemoryModel& operator=(const SingleMemoryModel&) = delete;
public:
	~SingleMemoryModel()
	{
		std::cout << "single auto delete success " << std::endl;
	}
	static std::shared_ptr<SingleMemoryModel> GetInst()
	{
		// 1 处
		if (_b_init.load(std::memory_order_acquire))
		{
			return single;
		}
		// 2 处
		s_mutex.lock();
		// 3 处
		if (_b_init.load(std::memory_order_relaxed))
		{
			s_mutex.unlock();
			return single;
		}
		// 4处
		single = std::shared_ptr<SingleMemoryModel>(new SingleMemoryModel);
		_b_init.store(true, std::memory_order_release);
		s_mutex.unlock();
		return single;
	}
private:
	static std::shared_ptr<SingleMemoryModel> single;
	static std::mutex s_mutex;
	static std::atomic<bool> _b_init;
};

std::shared_ptr<SingleMemoryModel> SingleMemoryModel::single = nullptr;
std::mutex SingleMemoryModel::s_mutex;
std::atomic<bool> SingleMemoryModel::_b_init = false;


void TestSingleMemory() {
	std::thread t1([]() {
		std::cout << "thread t1 singleton address is 0x: " << SingleMemoryModel::GetInst() << std::endl;
		});

	std::thread t2([]() {
		std::cout << "thread t2 singleton address is 0x: " << SingleMemoryModel::GetInst() << std::endl;
		});

	t2.join();
	t1.join();
}

int main()
{
	//TestOrderSeqCst();
	//TestOrderRelaxed();
	//TestReleaseAcquire();
	//ReleasAcquireDanger();
	//ReleaseSequence();
	//TestSingle();
	TestSingleMemory();
	std::cout << "Hello World!\n";
}


