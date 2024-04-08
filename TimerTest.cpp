#include "stdafx.h"
#include "TimerTest.h"

void CTimerTest::OnTimedEvent1()
{
    printf("\r\nTimer 1  Called (count=%i)", timer1.GetCount());
	/*if (_scene) {
		Model* model = _scene->getModel();

		if (model) {
			printf("get next line of animation data");
		}
	}*/
}

void CTimerTest::OnTimedEvent2()
{
    printf("\r\nTimer  2 Called (count=%i)", timer2.GetCount());
}

/*void CTimerTest::startTimer()
{
	timer1.SetTimedEvent(this, &CTimerTest::OnTimedEvent1);
	timer1.Start(1000); // Start timer 1 every 1s
}

void CTimerTest::stopTimer()
{
	timer1.Stop();      // Stop timer 1
}

void CTimerTest::setScene(Scene* scene)
{
	_scene = scene;
}*/

void CTimerTest::RunTest()
{
    printf("Hit return to start and stop timers");
    getchar();

    timer1.SetTimedEvent(this, &CTimerTest::OnTimedEvent1);
    timer1.Start(1000); // Start timer 1 every 1s

    timer2.SetTimedEvent(this, &CTimerTest::OnTimedEvent2);
    timer2.Start(1000); // Start timer 2 every 2s

    // Do something, in this case just wait for user to hit return   
    getchar();          // Wait for return (stop)

    timer1.Stop();      // Stop timer 1
    timer2.Stop();      // Stop timer 2

    printf("\r\nTimers stopped (hit return to exit)");
    getchar();
}
