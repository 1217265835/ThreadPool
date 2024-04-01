#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map>

// 任意数据类型，用于接收任意数据类型的返回值
class Any {
public:
	Any() = default;//默认构造函数
	~Any() = default;//析构函数
	template<typename T>
	Any(T data) : ptr_(std::make_unique< Derived<T>>(data)) {}//构造函数

	template<typename T>
	T cast() {
		auto derived = dynamic_cast<Derived<T>*>(ptr_.get());//将基类指针转换为派生类指针，此处要注意
		if (derived) {
			return derived->data_;
		}
		else {
			throw std::bad_cast();
		}
	}
private:
	//基类
	class Base {
	public:
		virtual ~Base() = default;
		
	};
	//派生类
	template<typename T>
	class Derived : public Base {
	public:
		Derived(T data) : data_(data) {}
		T data_;
	};

private:
	std::shared_ptr<Base> ptr_;//基类指针
};
//实现一个信号量机制
class Semaphore {
public:
	Semaphore(int count_ = 0) : count(count_) {}
	void post() {
		std::unique_lock<std::mutex> lock(mtx);
		count++;
		cv.notify_all();
	}
	void wait() {
		std::unique_lock<std::mutex> lock(mtx);
		while (count == 0) {
			cv.wait(lock);
		}
		count--;
	}
private:
	std::mutex mtx;
	std::condition_variable cv;
	int count;
};

class Task;
//实现用于接收任务返回数据的类,相比于Any,Result类多了一个信号量，用于通知主线程任务已经完成
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isVaild = true);
	~Result() = default;

	//setVal方法用于设置任务返回数据
	void setVal(Any any);
	//getVal方法用于获取任务返回数据
	Any getVal();

private:
	Any any_; //用于接收任务返回数据
	Semaphore sem_;//信号量
	std::shared_ptr<Task> task_; //指向对应任务的智能指针，用于获取任务返回数据
	std::atomic_bool isVaild_;//用于判断Result是否有效
};

// 任务抽象基类
//用户可以继承该类，实现自己的任务,重写run方法，实现具体的任务逻辑
class Task {
public:
	Task();
	~Task();
	virtual Any run() ; //任务执行入口
	void exec() ; //执行任务
	void setResult(Result *result); //设置任务返回数据
private:
	Result *result_; //任务返回数据
};

//线程类型
enum  PoolMode {
	MODE_FIXED,//固定线程池
	MODE_CACHED,//动态线程池
};
class Thread {
public:
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func);
	~Thread();
	void start(); //启动线程
	void join(); //等待线程结束
	int getThreadID()const;
	Thread(const Thread&) = delete; //禁止拷贝构造
	Thread& operator=(const Thread&) = delete; //禁止赋值构造
private:
	ThreadFunc func_; //线程函数
	static int generatedId_;
	int threadid_; //保存线程id
};
class ThreadPool{
public:
	//构造函数
	ThreadPool();
	//析构函数
	~ThreadPool();
	void setTaskQueMaxThreshold(int maxThreshold); //设置任务队列最大阈值
	void setThreadMAxThreshold(int maxThreshold); //设置线程最大阈值
	Result submitTask(std::shared_ptr<Task> task); //提交任务
	void start(size_t initThreadSize = 8); //启动线程池
	void setPoolMode(PoolMode mode); //设置线程池模式
	
	ThreadPool(const ThreadPool&) = delete; //禁止拷贝构造
	ThreadPool& operator=(const ThreadPool&) = delete; //禁止赋值构造
private:
	void ThreadFunc(int threadid); //线程函数
	bool checkRunningState() const;
private:
	/*
	* 此处不要放线程对象，而要使用指针，因为线程对象是不可拷贝的，会导致编译错误
	*/
	std::unordered_map<int,std::unique_ptr<Thread>> _threads;//线程列表，
	//std::vector<std::unique_ptr<Thread>> _threads; //线程列表,此处可以使用智能指针
	size_t initThreadSize_; //初始化线程数量
	std::queue<std::shared_ptr<Task>> Taskque; //任务队列,传入基类指针，因为不同的任务是用多态实现的，所以需要用基类指针
	std::atomic_uint  taskSize_; //任务数量
	int  taskQueMaxThreshold_; //任务队列最大阈值
	std::mutex taskQueMtx; //保证任务队列的线程安全
	std::condition_variable notEmpty; //任务队列不为空的条件变量
	std::condition_variable notFull; //任务队列不满的条件变量
	PoolMode mode_; //线程池模式
	std::atomic_bool isRunning;//判断线程池是否在运行
	std::atomic_int idleThreadSize_; //空闲线程阈值
	int threadMaxThreshHold_; //线程数量的上限
	std::atomic_int curThreadSize_; //当前线程数量
	std::mutex taskQueMtx2; //保证任务队列的线程安全
	std::condition_variable exitable;//线程资源全部回收
};


#endif // THREAD_POOL_H

