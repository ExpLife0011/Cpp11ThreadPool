#pragma once
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <atomic>
#include <future>
#include <stdexcept>

namespace mystd
{
	//�̳߳��������,Ӧ������Сһ��
#define  THREADPOOL_MAX_NUM 16
	//#define  THREADPOOL_AUTO_GROW
	using namespace std;
	//�̳߳�,�����ύ��κ�������ķ����ʽ����������ִ��,���Ի�ȡִ�з���ֵ
	//��ֱ��֧�����Ա����, ֧���ྲ̬��Ա������ȫ�ֺ���,Opteron()������
	class threadpool
	{
		using Task = function<void()>;	//��������
		vector<thread> _pool;			//�̳߳�
		queue<Task> _tasks;            //�������
		mutex _lock;                   //ͬ��
		condition_variable _task_cv;   //��������
		atomic<bool> _run{ true };     //�̳߳��Ƿ�ִ��
		atomic<int>  _ldleThread{ 0 };  //�����߳�����

	public:
		inline threadpool(unsigned short size = 4) { addThread(size); }
		inline ~threadpool()
		{
			_run = false;
			_task_cv.notify_all(); // ���������߳�ִ��
			for (thread& thread : _pool) {
				//thread.detach(); // ���̡߳���������
				if (thread.joinable())
					thread.join(); // �ȴ���������� ǰ�᣺�߳�һ����ִ����
			}
		}
		
	public:
		// �ύһ������
		// ����.get()��ȡ����ֵ��ȴ�����ִ����,��ȡ����ֵ
		// �����ַ�������ʵ�ֵ������Ա��
		// һ����ʹ��   bind�� .commit(std::bind(&Dog::sayHello, &dog));
		// һ������   mem_fn�� .commit(std::mem_fn(&Dog::sayHello), this)
		template<class F, class... Args>
		auto commit(F&& f, Args&&... args) ->future<decltype(f(args...))>
		{
			if (!_run)    // stoped ??
				throw runtime_error("commit on ThreadPool is stopped.");
			// typename std::result_of<F(Args...)>::type, ���� f �ķ���ֵ����
			using RetType = decltype(f(args...));
			auto task = make_shared<packaged_task<RetType()>>(
				bind(forward<F>(f), forward<Args>(args)...)
				);
			// �Ѻ�����ڼ�����,���(��)
			std::future<RetType> future = task->get_future();
			{
				// ������񵽶���
				//�Ե�ǰ���������  lock_guard �� mutex �� stack ��װ�࣬�����ʱ�� lock()��������ʱ�� unlock()
				lock_guard<mutex> lock{ _lock };
				// push(Task{...}) �ŵ����к���
				_tasks.emplace([task](){
					(*task)();
				});
			}
#ifdef THREADPOOL_AUTO_GROW
			if (_ldleThread < 1 && _pool.size() < THREADPOOL_MAX_NUM)
				addThread(1);
#endif // !THREADPOOL_AUTO_GROW
			_task_cv.notify_one(); // ����һ���߳�ִ��

			return future;
		}

		//�����߳�����
		int LdleThreadPoolSize() { return _ldleThread; }
		int ThreadPoolSize() { return _pool.size(); }
#ifndef THREADPOOL_AUTO_GROW
	private:
#endif // !THREADPOOL_AUTO_GROW
		//���ָ���������߳�
		void addThread(unsigned short size)
		{
			//�����߳�����,�������� Ԥ�������� THREADPOOL_MAX_NUM
			for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size)
			{   
				// �����̺߳���
				_pool.emplace_back([this]{ 
					while (_run)
					{
						// ��ȡһ����ִ�е� task
						Task task; 
						{
							// unique_lock ��� lock_guard �ĺô��ǣ�������ʱ unlock() �� lock()
							unique_lock<mutex> lock{ _lock };
							_task_cv.wait(lock, [this]{
								return !_run || !_tasks.empty();
							});
							// wait ֱ���� task
							if (!_run && _tasks.empty())
								return;
							// ���Ƚ��ȳ��Ӷ���ȡһ�� task
							task = move(_tasks.front());
							_tasks.pop();
						}
						_ldleThread--;
						//ִ������
						task();
						_ldleThread++;
					}
				});
				_ldleThread++;
			}
		}
	};

}

#endif  

//https://github.com/lzpong/
