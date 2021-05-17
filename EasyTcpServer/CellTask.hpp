#ifndef _CELL_TASK_H_
#include<Thread>
#include<mutex>
#include<list>


class CellTask
{
public:
	CellTask() {

	}

	virtual ~CellTask() {

	}

	virtual void doTask() {}
private:

};


class CellTaskServer
{
public:
	CellTaskServer()
	{

	}

	~CellTaskServer()
	{

	}

	//添加任务
	void addTask(CellTask* task)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_tasksBuf.push_back(task);
	}

	//启动工作线程
	void Start()
	{
		std::thread t(&CellTaskServer::OnRun, this);
		t.detach();
	}

private:
	//工作函数
	void OnRun()
	{
		while (true)
		{
			if (!_tasksBuf.empty())
			{
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pTask : _tasksBuf)
				{
					_tasks.push_back(pTask);
				}
				_tasksBuf.clear();
			}

			if (_tasks.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}

			for (auto pTask : _tasks)
			{
				pTask->doTask();
				delete pTask;
			}

			//清空任务
			_tasks.clear();
		}
	}

private:
	//任务数据
	std::list<CellTask*> _tasks;
	//任务数据缓冲区
	std::list<CellTask*> _tasksBuf;
	std::mutex _mutex;
};
#endif // !_CELL_TASK_H_
