// ThreadPool.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<functional>

#include"threadpool.h"

const int TASKQUE_MAX_THRESHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;
//任务类方法
Task::Task()
    :result_(nullptr)
{}
Task::~Task()
{}
Any Task::run()
{
    std::cout << "任务执行" << std::endl;
    return "aa";
}
void Task::exec()
{
    if (result_ != nullptr)
    {
        result_->setVal(run()); //执行任务，任务结果放入result对象中
    }
}
void Task::setResult(Result* result)
{
    result_ = result;
}

//构造函数
ThreadPool::ThreadPool()
    :initThreadSize_(4)
    , taskSize_(0)
    , taskQueMaxThreshold_(TASKQUE_MAX_THRESHOLD)
    ,threadMaxThreshHold_(THREAD_MAX_THRESHHOLD)
    , mode_(MODE_FIXED)
    , isRunning(true)
    ,idleThreadSize_(0)
    ,curThreadSize_(0)
{}
//析构函数
ThreadPool::~ThreadPool()
{
    //没有new就不用析构
    isRunning = false;
    notEmpty.notify_all();
   // 等待所有任务返回 ，有两种状态，阻塞&执行中
    std::unique_lock<std::mutex> lock(taskQueMtx);
    exitable.wait(lock, [&]()->bool {return _threads.size() == 0; });

}
//设置任务队列最大阈值
void ThreadPool::setTaskQueMaxThreshold(int maxThreshold)
{
    if (checkRunningState()) {
        return;
    }
	taskQueMaxThreshold_ = maxThreshold;
}
//设置线程数量最大阈值
void ThreadPool::setThreadMAxThreshold(int maxThreshold) {
    if (checkRunningState()) {
        return;
    }
    if(mode_ == PoolMode::MODE_FIXED) threadMaxThreshHold_ = maxThreshold;
}
//设置线程池模式
void ThreadPool::setPoolMode(PoolMode mode)
{
    if (isRunning == true) {
        mode_ = mode;
    }
	
}

bool ThreadPool::checkRunningState() const {
    return isRunning;
}

//向任务队列中提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
    std::unique_lock<std::mutex> lock(taskQueMtx);
    if (Taskque.size() >= taskQueMaxThreshold_)
    {
        if (notFull.wait_for(lock, std::chrono::seconds(1))==std::cv_status::timeout )
        {
			std::cout << "任务队列已满，提交任务失败" << std::endl;
      
            return Result(task,false);
		}
	}
    Taskque.push(task);
    taskSize_++;
    
    //return task->getResult();//这种是Result对象是属于task的，task在被线程执行完毕后会被释放
                                            //这种不行，在task析构时，会释放result对象，导致result对象被释放
    if (mode_ == PoolMode::MODE_CACHED
        && taskSize_ > idleThreadSize_
        && curThreadSize_ < threadMaxThreshHold_) 
    {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this,std::placeholders::_1));
        int threadID = ptr->getThreadID();
        _threads.emplace(threadID,std::move(ptr));
        _threads[threadID]->start();
        curThreadSize_++;
        idleThreadSize_++;
    }
    notEmpty.notify_one();
    return Result(task,true);//这种是Result对象是属于ThreadPool的，不会被释放
    
}

//启动线程池
void ThreadPool::start(size_t initThreadSize)
{
    //设置线程池的初始运行状态
    isRunning = true;
    //设置初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;
	//创建线程
    for (size_t i = 0; i < initThreadSize_; i++)
    {
       
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::ThreadFunc, this, std::placeholders::_1));
        std::cout << "threadid:" << ptr->getThreadID() << "create()" << std::endl;
        int threadId = ptr->getThreadID();
		//_threads.push_back(std::move(ptr));
        _threads.emplace(threadId, std::move(ptr));
	}
    //启动线程
    for (size_t i = 0; i < initThreadSize_; i++)
    {
		_threads[i]->start();
        //设置空闲线程的数量
        idleThreadSize_ ++;
	}
}
int Thread::generatedId_ = 0;
void ThreadPool::ThreadFunc(int threadid)
{
    auto lastTime = std::chrono::high_resolution_clock().now();
    std::cout << "ThreadFunc函数开始执行,本线程号为"<<std::this_thread::get_id() <<"\n" << std::endl;
    std::cout << "现在有" << this->curThreadSize_<<"个线程" << "\n" << std::endl;

    while (isRunning == true)
    {
		std::shared_ptr<Task> task;

        //此处代码块用于减小锁的作用域，提高效率
        {
            std::unique_lock<std::mutex> lock(taskQueMtx);
            //cached模式下，有可能已经创建了很多线程，但是空闲时间超过60s，应该把多余的线程
            //结束回收掉（超过initThreadSize_数量的线程要进行回收）
            while (Taskque.size() == 0) {
                if (mode_ == PoolMode::MODE_CACHED) {
                    if (std::cv_status::timeout ==
                        notEmpty.wait_for(lock, std::chrono::seconds(1)))//条件变量超时访问
                    {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (dur.count() >= THREAD_MAX_IDLE_TIME
                            && curThreadSize_ > initThreadSize_) {
                            //开始回收线程
                            _threads.erase(threadid); //std::this_thread::getid();
                            curThreadSize_--;
                            idleThreadSize_--;
                            std::cout << "threadid:" << std::this_thread::get_id() << "exit()";
                            return;
                        }
                    }
                }
                else {
                    notEmpty.wait(lock);
                }
                if(isRunning == false)//线程池关闭，回收资源
				{
                    _threads.erase(threadid); //std::this_thread::getid();
                    curThreadSize_--;
                    idleThreadSize_--;
                    std::cout << "threadid:" << std::this_thread::get_id() << "exit()";
                    exitable.notify_all();
                    return;
				}
             }
                        //设置空闲线程的数量
            idleThreadSize_ --;
            std::cout << "目前有" << taskSize_ << "个任务" << std::endl;
            task = Taskque.front();
            Taskque.pop();
            taskSize_--;
        }
		
        if (taskSize_< taskQueMaxThreshold_)
        {
			notFull.notify_one();
		}
		
        if (task)
        {
			//task->run();//执行任务，任务结果放入result对象中
            task->exec();
		}
       
        idleThreadSize_++;
        lastTime = std::chrono::high_resolution_clock().now();//更新线程执行完任务的时间
	}
    _threads.erase(threadid); //std::this_thread::getid();
    curThreadSize_--;
    idleThreadSize_--;
    exitable.notify_all();
    std::cout << "threadid:" << std::this_thread::get_id() << "exit()";
    std::cout<<"ThreadFunc函数执行完毕,本线程号为"<<std::this_thread::get_id() <<"\n" << std::endl;
   
}
///////线程类方法

//构造函数
Thread::Thread(ThreadFunc func)
	:func_(func)
    ,threadid_(generatedId_++)
{}
//析构函数
Thread::~Thread()
{}
//启动线程
void Thread::start()
{
	//创建线程来执行函数
	std::thread t(func_, threadid_);
  
	t.detach();//设置函数分离，避免函数执行完毕后，线程对象作用域完毕线程被销毁，导致函数无法执行
    //设置分离后，线程对象、线程id都会被销毁，但是线程函数会继续执行

}
int  Thread::getThreadID()const
{
    return threadid_;
}


//Result类方法
Result::Result(std::shared_ptr<Task> task, bool isVaild)
	:task_(task)
	,isVaild_(isVaild)
{
    task_->setResult(this);
}



Any Result::getVal()
{
    if (!isVaild_)
    {
		return "";
	}
	sem_.wait();
	return std::move(any_);
}
void Result::setVal(Any any)
{
	any_ = any;
	sem_.post();
}

