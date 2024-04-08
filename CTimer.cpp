#include "stdafx.h"
#include "CTimer.h"
#include "signal.hpp"
#include <iostream>

void f() { 
	std::cout << "free function\n"; 
}

struct o {
	void operator()() { std::cout << "function object\n"; }
};

auto lambda = []() { 
	std::cout << "lambda\n"; 
};

// declare a signal instance with no arguments
sigslot::signal<> sigs;

void CTimerTest::OnTimedEvent1()
{
	sigs.connect(f);
}

void CTimerTest::OnTimedEvent2()
{
	sigs.connect(lambda);
}

void CTimerTest::TimerTest()
{
	timer1.SetTimedEvent(this, &CTimerTest::OnTimedEvent1);
	timer1.Start(1000);
	timer2.SetTimedEvent(this, &CTimerTest::OnTimedEvent2);
	timer2.Start(2000);
}

// emit a signal
sigs();
