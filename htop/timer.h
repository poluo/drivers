#ifndef _TIMER_H
#define _TIMER_H

#define TIMER_SIG SIGRTMAX
extern timer_t init_timer();
extern void del_timer(timer_t timerid);
#endif