#include "TemplateTimer.h"

class CTimerTest
{
public:
	void TimerTest();
private:
	void OnTimedEvent1();
	void OnTimedEvent2();
	TTimer<CTimerTest> timer1;
	TTimer<CTimerTest> timer2;
};