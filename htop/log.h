#ifndef _LOG_H
#define _LOG_H

#include <time.h>

#define LOG_DATE_LENGTH 26

#define debug(args...) 	debug_int(__FILE__, __FUNCTION__, __LINE__, ##args)
#define info(args...)    info_int(__FILE__, __FUNCTION__, __LINE__, ##args)
#define error(args...)   error_int( __FILE__, __FUNCTION__, __LINE__, ##args)
#define fatal(args...)   fatal_int(__FILE__, __FUNCTION__, __LINE__, ##args)

extern void log_date(char *buf, size_t len);
extern void debug_int( const char *file, const char *function, const unsigned long line, const char *fmt, ... );
extern void info_int( const char *file, const char *function, const unsigned long line, const char *fmt, ... );
extern void error_int(const char *file, const char *function, const unsigned long line, const char *fmt, ... );
extern void fatal_int( const char *file, const char *function, const unsigned long line, const char *fmt, ... );

#endif