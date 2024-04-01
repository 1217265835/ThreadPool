#include <iostream>
#include<thread>
#include<chrono>
#include<mutex>
#include<string>
#include"threadpool.h"


class myTask : public Task
{
public:
    myTask(int begin, int end,std::string a) 
        :begin_(begin)
        ,end_(end)
        ,str(a)
    {}
    Any run() //run�������վͻ����̳߳ط�����߳���ִ��
    {
  
        std::cout << "�߳�"<<str << std::this_thread::get_id() << "��ȡ����ɹ�,ִ������" << std::endl;

        int res = 0;
        for (int i = begin_; i < end_; i++)
        {
			res += i;
		}
      
		std::cout << "����ִ�н��������Ϊ"<<res << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(3));
        return res;
	}
private:
    int begin_;
    int end_;
    std::string str;
};

int main()
{
    ThreadPool pool;
    //�����̳߳�ģʽ
    pool.setPoolMode(PoolMode::MODE_CACHED);
   
    //�����̳߳�
    pool.start(4);
    std::shared_ptr<Task> p(new myTask(1000,2000,"aaa"));
    std::shared_ptr<Task> p2(new myTask(2000, 3000,"bbb"));
    std::shared_ptr<Task> p3(new myTask(3000, 4000,"ccc"));
    std::shared_ptr<Task> p4(new myTask(4000, 5000,"ddd"));
    std::shared_ptr<Task> p5(new myTask(5000, 6000,"eee"));
    std::shared_ptr<Task> p6(new myTask(7000, 8000,"fff"));
    Result res = pool.submitTask(std::move(p));   
    Result res2 = pool.submitTask(std::move(p2));
    Result res3 = pool.submitTask(std::move(p3));
    Result res4 = pool.submitTask(std::move(p4));
    Result res5 = pool.submitTask(std::move(p5));
    Result res6 = pool.submitTask(std::move(p6));
   // std::this_thread::sleep_for(std::chrono::seconds(10));
}