#ifndef _TOOLS_H
#define _TOOLS_H

#define SEC_PER_DAY  (3600 * 24)
extern void get_HZ(void);
extern void init_timespec_val(struct timespec *v, time_t sec, long nsec);
extern time_t get_local_time(struct tm *rectime,int day_off);
#endif