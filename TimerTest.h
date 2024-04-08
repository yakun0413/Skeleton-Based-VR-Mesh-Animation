#pragma once
#include "TemplateTimer.h"
class CTimerTest
{
public:
	void BeginTest();
	void RunTest();
	void StopTest();
private:
	void OnTimedEvent0();
	void OnTimedEvent1();
	void OnTimedEvent2();
	void OnTimedEvent3();
	void OnTimedEvent4();

	//TTimer<CTimerTest> timer;
	TTimer<CTimerTest> timer0;
	TTimer<CTimerTest> timer1;
	TTimer<CTimerTest> timer2;
	TTimer<CTimerTest> timer3;
	TTimer<CTimerTest> timer4;
};
