#include <iostream>
#include <thread>
#include <mutex>
#include <memory>

//早期单例模式
//C++11 以前该方式存在风险，在多个线程初始化时存在开辟多个实例情况
//C++ 11以后大部分的单例都回归到这个模式了
class Single2 {
private:
	Single2()
	{
	}
	Single2(const Single2&) = delete;
	Single2& operator=(const Single2&) = delete;
public:
	static Single2& GetInst()
	{
		static Single2 single;
		return single;
	}
};

void test_single2() {
	//多线程情况下可能存在问题
	std::cout << "s1 addr is " << &Single2::GetInst() << std::endl;
	std::cout << "s2 addr is " << &Single2::GetInst() << std::endl;
}





//C++11 标准以前大家探讨了很多种实现方式，包括下面的饿汉式方式，
//在程序启动后优先用一个线程初始化资源，这样即使之后使用多线程访问也不会出问题。
//但这种方式释放存在问题，可能会被多个线程释放或不知道用哪个线程释放。

//饿汉式
class Single2Hungry
{
private:
	Single2Hungry()
	{
	}
	Single2Hungry(const Single2Hungry&) = delete;
	Single2Hungry& operator=(const Single2Hungry&) = delete;
public:
	static Single2Hungry* GetInst()
	{
		if (single == nullptr)
		{
			single = new Single2Hungry();
		}
		return single;
	}
private:
	static Single2Hungry* single;
};

//饿汉式初始化
Single2Hungry* Single2Hungry::single = Single2Hungry::GetInst();

void thread_func_s2(int i)
{
	std::cout << "this is thread " << i << std::endl;
	std::cout << "inst is " << Single2Hungry::GetInst() << std::endl;
}
void test_single2hungry()
{
	std::cout << "s1 addr is " << Single2Hungry::GetInst() << std::endl;
	std::cout << "s2 addr is " << Single2Hungry::GetInst() << std::endl;
	for (int i = 0; i < 3; i++)
	{
		std::thread tid(thread_func_s2, i);
		tid.join();
	}
}





//如果用户期望使用懒汉式就需要加锁
//但是回收指针同样存在问题，存在多重释放或者不知道哪个指针释放的问题。
class SinglePointer
{
private:
	SinglePointer()
	{
	}
	SinglePointer(const SinglePointer&) = delete;
	SinglePointer& operator=(const SinglePointer&) = delete;
public:
	static SinglePointer* GetInst()
	{
		if (single != nullptr)
		{
			return single;
		}
		s_mutex.lock();
		if (single != nullptr)
		{
			s_mutex.unlock();
			return single;
		}
		single = new SinglePointer();
		s_mutex.unlock();
		return single;
	}
private:
	static SinglePointer* single;
	static std::mutex s_mutex;
};
SinglePointer* SinglePointer::single = nullptr;
std::mutex SinglePointer::s_mutex;

void thread_func_lazy(int i)
{
	std::cout << "this is lazy thread " << i << std::endl;
	std::cout << "inst is " << SinglePointer::GetInst() << std::endl;
}
void test_singlelazy()
{
	for (int i = 0; i < 3; i++)
	{
		std::thread tid(thread_func_lazy, i);
		tid.join();
	}
	//何时释放new的对象？造成内存泄漏
}






//可以利用智能指针完成自动回收

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
		if (single != nullptr)
		{
			return single;
		}
		s_mutex.lock();
		if (single != nullptr)
		{
			s_mutex.unlock();
			return single;
		}
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

void test_singleauto()
{
	auto sp1 = SingleAuto::GetInst();
	auto sp2 = SingleAuto::GetInst();
	std::cout << "sp1  is  " << sp1 << std::endl;
	std::cout << "sp2  is  " << sp2 << std::endl;
	//此时存在隐患，可以手动删除裸指针，造成崩溃
	// delete sp1.get();
}






//为了规避用户手动释放内存，可以提供一个辅助类帮忙回收内存
//并将单例类的析构函数写为私有

class SingleAutoSafe;
class SafeDeletor
{
public:
	void operator()(SingleAutoSafe* sf)
	{
		std::cout << "this is safe deleter operator()" << std::endl;
		delete sf;
	}
};

class SingleAutoSafe
{
private:
	SingleAutoSafe() {}
	~SingleAutoSafe()
	{
		std::cout << "this is single auto safe deletor" << std::endl;
	}
	SingleAutoSafe(const SingleAutoSafe&) = delete;
	SingleAutoSafe& operator=(const SingleAutoSafe&) = delete;
	//定义友元类，通过友元类调用该类析构函数
	friend class SafeDeletor;
public:
	static std::shared_ptr<SingleAutoSafe> GetInst()
	{
		//1处
		if (single != nullptr)
		{
			return single;
		}
		s_mutex.lock();
		//2处
		if (single != nullptr)
		{
			s_mutex.unlock();
			return single;
		}
		//额外指定删除器  
		//3 处
		single = std::shared_ptr<SingleAutoSafe>(new SingleAutoSafe, SafeDeletor());
		//也可以指定删除函数
		// single = std::shared_ptr<SingleAutoSafe>(new SingleAutoSafe, SafeDelFunc);
		s_mutex.unlock();
		return single;
	}
private:
	static std::shared_ptr<SingleAutoSafe> single;
	static std::mutex s_mutex;
};

std::shared_ptr<SingleAutoSafe> SingleAutoSafe::single = nullptr;
std::mutex SingleAutoSafe::s_mutex;



//但是上面的代码存在危险，比如懒汉式的使用方式，当多个线程调用单例时，有一个线程加锁进入3处的逻辑。
//其他的线程有的在1处，判断指针非空则跳过初始化直接使用单例的内存会存在问题。
//主要原因在于SingleAutoSafe * temp =  new SingleAutoSafe()  这个操作是由三部分组成的
//1 调用allocate开辟内存
//2 调用construct执行SingleAutoSafe的构造函数
//3 调用赋值操作将地址赋值给temp

//而现实中2和3的步骤可能颠倒，所以有可能在一些编译器中通过优化是1，3，2的调用顺序，
//其他线程取到的指针就是非空，还没来的及调用构造函数就交给外部使用造成不可预知错误。





//为解决这个问题，C++11 推出了std::call_once函数保证多个线程只执行一次

class SingletonOnce {
private:
	SingletonOnce() = default;
	SingletonOnce(const SingletonOnce&) = delete;
	SingletonOnce& operator = (const SingletonOnce& st) = delete;
	static std::shared_ptr<SingletonOnce> _instance;

public:
	static std::shared_ptr<SingletonOnce> GetInstance() {
		static std::once_flag s_flag;
		std::call_once(s_flag, [&]() {
			_instance = std::shared_ptr<SingletonOnce>(new SingletonOnce);
			});

		return _instance;
	}

	void PrintAddress() {
		std::cout << _instance.get() << std::endl;
	}

	~SingletonOnce() {
		std::cout << "this is singleton destruct" << std::endl;
	}
};

std::shared_ptr<SingletonOnce> SingletonOnce::_instance = nullptr;

void TestSingle() {

	std::thread t1([]() {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		SingletonOnce::GetInstance()->PrintAddress();
		});

	std::thread t2([]() {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		SingletonOnce::GetInstance()->PrintAddress();
		});

	t1.join();
	t2.join();
}

//为了让单例更加通用，可以做成模板类
template <typename T>
class Singleton {
protected:
	Singleton() = default;
	Singleton(const Singleton<T>&) = delete;
	Singleton& operator=(const Singleton<T>& st) = delete;
	static std::shared_ptr<T> _instance;
public:
	static std::shared_ptr<T> GetInstance() {
		static std::once_flag s_flag;
		std::call_once(s_flag, [&]() {
			_instance = std::shared_ptr<T>(new T);
			});
		return _instance;
	}
	void PrintAddress() {
		std::cout << _instance.get() << std::endl;
	}
	~Singleton() {
		std::cout << "this is singleton destruct" << std::endl;
	}
};
template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;

//想使用单例类，可以继承上面的模板，我们在网络编程中逻辑单例类用的就是这种方式
class LogicSystem :public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem() {}
private:
	LogicSystem() {}
};

//如果你只是实现一个简单的单例类推荐使用返回局部静态变量的方式
//如果想大规模实现多个单例类可以用上述模板类

int main()
{
	//test_single2hungry();
	//test_singlelazy();
	TestSingle();
	//SingleAutoSafe::GetInst();
}


