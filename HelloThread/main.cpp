#include<iostream>
#include<thread>
#include<mutex> //��
#include<atomic> //ԭ��
using namespace std;

mutex m;
atomic<int> sum = 0;

void workFun(int index)
{
	//lock_guard<mutex> lg(m); //�Խ���������������ʼ�������˳���������Զ�����
	//m.lock(); //�ٽ���
	sum++;
	//m.unlock();
	cout << "sum=" <<sum<< endl;
}

int main() //main�����ᱻĬ�ϵ�����ǰ��������߳�
{
	thread* t[3];
	for (int n = 0; n < 3; n++)
	{
		t[n] = new thread(workFun, n);
		(*t[n]).detach();
	}
	//t.detach(); //���߳������߳��̷߳���ִ�У����ǵ����߳̽��������߳�Ҳ�ᱻ�Ƚ���
	//t.join(); //���߳�ִ�н���֮��ŷ������̼߳���ִ�У������̱߳����������Ǵ��������Ķ�����߳�֮���ǿ��Բ���ִ�е�
	cout << "Hello, I'm main thread"<<endl;
	return 0;
}