
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/timer.h>
#include "common.h"

//LDD 7.4
static struct timer_list timer;
static int timer_data = 0; //timer data must be global variable

void timer_handler(unsigned long data)
{
    PDEBUG(": %s()\n",__FUNCTION__);
    PDEBUG(": data %d\n",*((int *)data));
}
void test_timer(int data)
{

	init_timer(&timer);
    timer.expires = jiffies + 2*HZ;
    timer.function = timer_handler;
    timer.data = (unsigned long)&timer_data;
    add_timer(&timer);
    timer_data = data;
}

//LDD3 7.3
void test_sleep_method(int to_test)
{
	wait_queue_head_t wait;
	switch(to_test)
	{
		#if 0
		case 1:
			PDEBUG("First method, use Busy waiting, sleep 100ms:\n");
			PDEBUG(": jiffies %ld\n",jiffies);
			while(time_before(jiffies, jiffies + HZ/10))
				cpu_relax();
			PDEBUG("test cpu_relax() ok\n");
			PDEBUG(": jiffies %ld\n",jiffies);
			break;

		case 2:
			PDEBUG("Second method use yield the processor, sleep 1s\n");
			while(time_before(jiffies, jiffies + HZ))
				schedule();
			PDEBUG("test shcedule() ok\n");
			PDEBUG(": jiffies %ld\n",jiffies);
			break;
		#endif
		case 3:
			PDEBUG("Third methos use timeout to sleep 1s\n");
			PDEBUG(": jiffies %ld\n",jiffies);
			
			init_waitqueue_head(&wait);
			wait_event_interruptible_timeout(wait, 0, HZ);
			PDEBUG(": jiffies %ld\n",jiffies);
			break;

		case 4:
			PDEBUG("Third methos use timeout to sleep 1s\n");
			PDEBUG(": jiffies %ld\n",jiffies);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ);
			PDEBUG(": jiffies %ld\n",jiffies);
			break;

		default:
			PDEBUG("Use mdelay() to sleep 100ms\n");
			mdelay(500);    
			PDEBUG(": jiffies %ld\n",jiffies);
			break;
	}
	udelay(100);
}
//LDD3 7.1
void print_time(void)
{
	struct timeval tv;
    struct timespec ts;
	unsigned long j;

    ts.tv_sec = 10;
    ts.tv_nsec = 0;

    tv.tv_sec = 10;
    tv.tv_usec = 0;

    j = timespec_to_jiffies(&ts);
    PDEBUG("timespec.tv_sec = %ld tv_nsec = %ld, jiffies = %ld\n", ts.tv_sec, ts.tv_nsec, j);
    j = timeval_to_jiffies(&tv);
    PDEBUG("timespec.tv_sec = %ld tv_nsec = %ld, jiffies = %ld\n", tv.tv_sec, tv.tv_usec, j);

    j = jiffies;
    memset(&ts, 0, sizeof(struct timespec));
    memset(&tv, 0, sizeof(struct timeval));
    jiffies_to_timespec(j, &ts);
    PDEBUG("timespec.tv_sec = %ld tv_nsec = %ld, jiffies = %ld\n", ts.tv_sec, ts.tv_nsec, j);
    jiffies_to_timeval(j, &tv);
    PDEBUG("timeval.tv_sec = %ld tv_usec = %ld, jiffies = %ld\n", tv.tv_sec, tv.tv_usec, j);

    do_gettimeofday(&tv);
    PDEBUG("do_gettimeofday timeval.tv_sec = %ld tv_usec = %ld\n", tv.tv_sec, tv.tv_usec);
    ts = current_kernel_time();
    PDEBUG("current_kernel_time timespec.tv_sec = %ld tv_nsec = %ld\n", ts.tv_sec, ts.tv_nsec);

}
