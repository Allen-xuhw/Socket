#include<iostream>
#include<thread>
#include<mutex> //锁
#include<atomic> //原子
using namespace std;

mutex m;
atomic<int> sum = 0;

void workFun(int index)
{
	//lock_guard<mutex> lg(m); //自解锁，从申明处开始加锁，退出作用域后自动解锁
	//m.lock(); //临界区
	sum++;
	//m.unlock();
	cout << "sum=" <<sum<< endl;
}

int main() //main函数会被默认当作当前程序的主线程
{
	thread* t[3];
	for (int n = 0; n < 3; n++)
	{
		t[n] = new thread(workFun, n);
		(*t[n]).detach();
	}
	//t.detach(); //主线程与子线程线程分离执行，但是当主线程结束，子线程也会被迫结束
	//t.join(); //子线程执行结束之后才返回主线程继续执行，即主线程被阻塞；但是创建出来的多个子线程之间是可以并行执行的
	cout << "Hello, I'm main thread"<<endl;
	return 0;
}