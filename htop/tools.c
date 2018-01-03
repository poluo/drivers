#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include "tools.h"

extern unsigned int hz;
void get_HZ(void)
{
	long ticks;

	if ((ticks = sysconf(_SC_CLK_TCK)) == -1) {
		printf("sysconf");
	}

	hz = (unsigned int) ticks;
}

void init_timespec_val(struct timespec *v, time_t sec, long nsec)
{
	v->tv_sec = sec;
	v->tv_nsec = nsec;
}