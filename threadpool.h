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

// �����������ͣ����ڽ��������������͵ķ���ֵ
class Any {
public:
	Any() = default;//Ĭ�Ϲ��캯��
	~Any() = default;//��������
	template<typename T>
	Any(T data) : ptr_(std::make_unique< Derived<T>>(data)) {}//���캯��

	template<typename T>
	T cast() {
		auto derived = dynamic_cast<Derived<T>*>(ptr_.get());//������ָ��ת��Ϊ������ָ�룬�˴�Ҫע��
		if (derived) {
			return derived->data_;
		}
		else {
			throw std::bad_cast();
		}
	}
private:
	//����
	class Base {
	public:
		virtual ~Base() = default;
		
	};
	//������
	template<typename T>
	class Derived : public Base {
	public:
		Derived(T data) : data_(data) {}
		T data_;
	};

private:
	std::shared_ptr<Base> ptr_;//����ָ��
};
//ʵ��һ���ź�������
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
//ʵ�����ڽ������񷵻����ݵ���,�����Any,Result�����һ���ź���������֪ͨ���߳������Ѿ����
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isVaild = true);
	~Result() = default;

	//setVal���������������񷵻�����
	void setVal(Any any);
	//getVal�������ڻ�ȡ���񷵻�����
	Any getVal();

private:
	Any any_; //���ڽ������񷵻�����
	Semaphore sem_;//�ź���
	std::shared_ptr<Task> task_; //ָ���Ӧ���������ָ�룬���ڻ�ȡ���񷵻�����
	std::atomic_bool isVaild_;//�����ж�Result�Ƿ���Ч
};

// ����������
//�û����Լ̳и��࣬ʵ���Լ�������,��дrun������ʵ�־���������߼�
class Task {
public:
	Task();
	~Task();
	virtual Any run() ; //����ִ�����
	void exec() ; //ִ������
	void setResult(Result *result); //�������񷵻�����
private:
	Result *result_; //���񷵻�����
};

//�߳�����
enum  PoolMode {
	MODE_FIXED,//�̶��̳߳�
	MODE_CACHED,//��̬�̳߳�
};
class Thread {
public:
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func);
	~Thread();
	void start(); //�����߳�
	void join(); //�ȴ��߳̽���
	int getThreadID()const;
	Thread(const Thread&) = delete; //��ֹ��������
	Thread& operator=(const Thread&) = delete; //��ֹ��ֵ����
private:
	ThreadFunc func_; //�̺߳���
	static int generatedId_;
	int threadid_; //�����߳�id
};
class ThreadPool{
public:
	//���캯��
	ThreadPool();
	//��������
	~ThreadPool();
	void setTaskQueMaxThreshold(int maxThreshold); //����������������ֵ
	void setThreadMAxThreshold(int maxThreshold); //�����߳������ֵ
	Result submitTask(std::shared_ptr<Task> task); //�ύ����
	void start(size_t initThreadSize = 8); //�����̳߳�
	void setPoolMode(PoolMode mode); //�����̳߳�ģʽ
	
	ThreadPool(const ThreadPool&) = delete; //��ֹ��������
	ThreadPool& operator=(const ThreadPool&) = delete; //��ֹ��ֵ����
private:
	void ThreadFunc(int threadid); //�̺߳���
	bool checkRunningState() const;
private:
	/*
	* �˴���Ҫ���̶߳��󣬶�Ҫʹ��ָ�룬��Ϊ�̶߳����ǲ��ɿ����ģ��ᵼ�±������
	*/
	std::unordered_map<int,std::unique_ptr<Thread>> _threads;//�߳��б�
	//std::vector<std::unique_ptr<Thread>> _threads; //�߳��б�,�˴�����ʹ������ָ��
	size_t initThreadSize_; //��ʼ���߳�����
	std::queue<std::shared_ptr<Task>> Taskque; //�������,�������ָ�룬��Ϊ��ͬ���������ö�̬ʵ�ֵģ�������Ҫ�û���ָ��
	std::atomic_uint  taskSize_; //��������
	int  taskQueMaxThreshold_; //������������ֵ
	std::mutex taskQueMtx; //��֤������е��̰߳�ȫ
	std::condition_variable notEmpty; //������в�Ϊ�յ���������
	std::condition_variable notFull; //������в�������������
	PoolMode mode_; //�̳߳�ģʽ
	std::atomic_bool isRunning;//�ж��̳߳��Ƿ�������
	std::atomic_int idleThreadSize_; //�����߳���ֵ
	int threadMaxThreshHold_; //�߳�����������
	std::atomic_int curThreadSize_; //��ǰ�߳�����
	std::mutex taskQueMtx2; //��֤������е��̰߳�ȫ
	std::condition_variable exitable;//�߳���Դȫ������
};


#endif // THREAD_POOL_H

