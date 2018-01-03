#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"

const char *program_name = "";
int debug_flags = 0;
int debug_log_syslog = 0;
int info_log_syslog = 1;
int error_log_syslog = 1;
int fatal_log_syslog = 1;

static inline time_t now_sec(clockid_t clk_id) 
{
    struct timespec ts;
    if(clock_gettime(clk_id, &ts) == -1)
        return 0;
    return ts.tv_sec;
}

//get time string
void log_date(char *buf, size_t len)
{
	time_t t;
	struct tm *tmp;
	if(!buf || !len)
		return;

	t = now_sec(CLOCK_REALTIME);
	tmp = localtime(&t);
	if(tmp == NULL)
	{
		buf[0] = '\0';
		return;
	}
	if(strftime(buf,len,"%Y-%m-%d %H:%M:%S",tmp) == 0)
	{
		buf[0] = '\0';
		return;
	}
	buf[len - 1] = '\0';
	return;
}

void syslog_init(void)
{
	static int once_time = 0;
	if(!once_time)
	{
		openlog(" ",LOG_PID|LOG_CONS, LOG_DAEMON);
		once_time = 1;
	}
}

void debug_int( const char *file, const char *function, const unsigned long line, const char *fmt, ... )
{
    va_list args;
	char date[LOG_DATE_LENGTH];
    log_date(date, LOG_DATE_LENGTH);

    if(debug_log_syslog) {
        va_start( args, fmt );
        vsyslog(LOG_DEBUG,  fmt, args );
        va_end(args);
    }

    va_start( args, fmt );
    if(debug_flags) 
    	fprintf(stderr, "%s: %s DEBUG: (%04lu@%-10.10s:%-15.15s): ", date, program_name, line, file, function);
    else            
    	fprintf(stderr, "%s: %s DEBUG: ", date, program_name);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}

void info_int( const char *file, const char *function, const unsigned long line, const char *fmt, ... )
{
    va_list args;

    if(info_log_syslog) {
        va_start( args, fmt );
        vsyslog(LOG_INFO,  fmt, args );
        va_end(args);
    }

    char date[LOG_DATE_LENGTH];
    log_date(date, LOG_DATE_LENGTH);

    va_start( args, fmt );
    if(debug_flags) 
    	fprintf(stderr, "%s: %s INFO: (%04lu@%-10.10s:%-15.15s): ", date, program_name, line, file, function);
    else            
    	fprintf(stderr, "%s: %s INFO: ", date, program_name);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}

void error_int(const char *file, const char *function, const unsigned long line, const char *fmt, ... ) 
{
    // save a copy of errno - just in case this function generates a new error
    int __errno = errno;

    va_list args;

    if(error_log_syslog) {
        va_start(args, fmt );
        vsyslog(LOG_ERR,  fmt, args);
        va_end(args );
    }

    char date[LOG_DATE_LENGTH];
    log_date(date, LOG_DATE_LENGTH);

    va_start( args, fmt );
    if(debug_flags) 
    	fprintf(stderr, "%s: %s ERROR: (%04lu@%-10.10s:%-15.15s): ", date, program_name, line, file, function);
    else            
    	fprintf(stderr, "%s: %s ERROR: ", date, program_name);
    vfprintf(stderr, fmt, args);
    va_end(args);

    if(__errno) {
        fprintf(stderr, " (errno %d, %s)\n", __errno, strerror(__errno));
        errno = 0;
    }
    else
        fputc('\n', stderr);
}

void cleanup_and_exit(int err)
{
	/*to do something more*/
	exit(err);
}

void fatal_int( const char *file, const char *function, const unsigned long line, const char *fmt, ... ) 
{
    va_list args;

    if(fatal_log_syslog) {
        va_start( args, fmt );
        vsyslog(LOG_CRIT,  fmt, args );
        va_end( args );
    }

    char date[LOG_DATE_LENGTH];
    log_date(date, LOG_DATE_LENGTH);


    va_start( args, fmt );
    if(debug_flags) 
    	fprintf(stderr, "%s: %s FATAL: (%04lu@%-10.10s:%-15.15s): ", date, program_name, line, file, function);
    else            
    	fprintf(stderr, "%s: %s FATAL: ", date, program_name);
    vfprintf(stderr, fmt, args );
    va_end( args );
    fputc('\n', stderr);

    cleanup_and_exit(1);
}