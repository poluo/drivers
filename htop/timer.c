#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "timer.h"

extern uint8_t enable_scan;

static void timer_handler(int sig,siginfo_t *si, void *uc)
{
	if(!enable_scan)
		enable_scan = 1;
	else
		enable_scan = 0;
}

timer_t init_timer()
{
	struct sigaction sa;
	struct itimerspec fs;
	struct sigevent sev;
	timer_t timerid; 
	
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = timer_handler;
	sigemptyset(&sa.sa_mask);
	
	if(sigaction(TIMER_SIG, &sa, NULL) == -1)
	{
		printf("sigaction error\n");
		exit(1);
	}

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = TIMER_SIG;
	sev.sigev_value.sival_int = 0; //int value accompanying signal
	sev.sigev_value.sival_ptr = NULL; //ptr value accompanying signal
	
	if(timer_create(CLOCK_REALTIME, &sev, &timerid) < 0)
	{
		printf("timer create error\n");
		exit(1);
	}
	
	fs.it_value.tv_sec = 0;
	fs.it_value.tv_nsec = 10;
	fs.it_interval.tv_sec = 5;
	fs.it_interval.tv_nsec = 0;

	if(timer_settime(timerid, 0, &fs, NULL) == -1)
	{
		printf("timer settime error\n");
		exit(1);
	}

	return timerid;
}

void del_timer(timer_t timerid)
{

	if(timer_delete(timerid) == -1)
	{
		printf("timer delete error\n");
	}
}