#ifndef _CELLTimestamp_hpp_
	#define _CELLTimestamp_hpp_

#include<chrono>
using namespace std::chrono;

class CELLTimestamp
{
public:
	CELLTimestamp()
	{
		update();
	}

	~CELLTimestamp()
	{

	}

	void update()
	{
		_begin = high_resolution_clock::now();
	}

	double getElapsedSecond()
	{
		return getElapsedTimeInMicrosSec() * 0.000001;
	}

	double getElapsedTimeInMilliSec()
	{
		return getElapsedTimeInMicrosSec() * 0.001;
	}

	double getElapsedTimeInMicrosSec()
	{
		return duration_cast<microseconds>(high_resolution_clock::now() - _begin).count();
	}

private:
	time_point<high_resolution_clock> _begin;
};

#endif