#ifndef _DRAW_H
#define _DRAW_H

#include <curses.h>

#define COLOR_DEFAULT_TEXT	COLOR_PAIR(1)
#define COLOR_ALERT_TEXT	COLOR_PAIR(2)
#define COLOR_CPU_USED_TEXT	COLOR_PAIR(3)
#define COLOR_WHITE_TEXT	COLOR_PAIR(4)
#define COLOR_PROCESS_HEADER	COLOR_PAIR(8)

#define printc(attrs,format, args...) \
do {								\
	if(strlen(format) < 0)			\
		continue;						\
  	attron(attrs);					\
   	printw(format, ##args);			\
	attroff(attrs);					\
   	refresh();						\
}while(0)

extern void draw_memory(void);
extern void draw_cpu(void);
extern void draw_process(void);


#endif
