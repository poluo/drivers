#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pwd.h>
#include "linux_info.h"
#include "draw.h"

extern cpuinfo cpu;
extern meminfo memory;
extern process_list_info process_list;
int row,col;

void draw_cpu(void)
{
	 static long long int us,sys,nice,idle,wait,irq,sirq,st;
	 double us_per,sys_per,nice_per,idle_per,wait_per,irq_per,sirq_per,st_per;
	
	 us_per = ((double)(cpu.usertime/cpu.cores - us) / cpu.period) * 100.0;
	 sys_per = ((double)(cpu.systemtime/cpu.cores - sys) / cpu.period) * 100.0;
	 nice_per = ((double)(cpu.nicetime/cpu.cores - nice) / cpu.period) * 100.0;
	 idle_per = ((double)(cpu.idletime/cpu.cores - idle) / cpu.period) * 100.0;
	 wait_per = ((double)(cpu.ioWait/cpu.cores - wait) / cpu.period) * 100.0;
	 irq_per = ((double)(cpu.irq/cpu.cores - irq) / cpu.period) * 100.0;
	 sirq_per = ((double)(cpu.softIrq/cpu.cores - sirq)/ cpu.period) * 100.0;
	 st_per = ((double)(cpu.steal/cpu.cores- st)/ cpu.period) * 100.0;
	 
	 printf("CPU: Model %s Cores %d load average ",cpu.model,cpu.cores);
	 printf("%.2lf %.2lf %.2lf\n",cpu.lavg1,cpu.lavg5,cpu.lavg15);

	 printf("CPU: us %.2lf sys %.2lf ni %.2lf id %.2f hi %.2f si %.2f st %.2f\n",
	 			us_per,sys_per,nice_per,idle_per,irq_per,sirq_per,st_per);
	 us = cpu.usertime/cpu.cores;
	 sys = cpu.systemtime/cpu.cores;
	 nice = cpu.nicetime/cpu.cores;
	 idle = cpu.idletime/cpu.cores;
	 wait = cpu.ioWait/cpu.cores;
	 irq = cpu.irq/cpu.cores;
	 sirq = cpu.softIrq/cpu.cores;
	 st = cpu.steal/cpu.cores;
}
void draw_memory(void)
{
	int unit = memory.totalMem / 50;
	int i,used = memory.usedMem / unit;

	printf("Mem: ");
	printf("Total %lld KB,  ",memory.totalMem);
    printf("Free %lld KB,  ", memory.freeMem);
	printf("Used %lld KB,  ", memory.usedMem);
    printf("Buffers %lld KB \n", memory.buffersMem);
	printf("Swap: ");
	printf("Total %lld KB,  ",memory.totalSwap);
    printf("Free %lld KB,  ", memory.freeSwap);
	printf("Used %lld KB,  ", memory.usedSwap);
    printf("Cached %lld KB \n", memory.cachedSwap);

}
void print_process_time(unsigned long long int totalHundredths) {
   unsigned long long totalSeconds = totalHundredths / 100;

   unsigned long long hours = totalSeconds / 3600;
   int minutes = (totalSeconds / 60) % 60;
   int seconds = totalSeconds % 60;
   int hundredths = totalHundredths - (totalSeconds * 100);

	if(hours)
	{
	   	printf("%lld:%d\t",hours,minutes);
	}
   	else
   	{
   		printf("%lld:%d.%d\t",hours,minutes,hundredths);
   	}
}

void draw_process(void)
{
	process_info *tmp;
	struct passwd *pw;
	
	printf("Task: %d running %d sleeping %d stoped %d zomie %d\n", 
		process_list.size,
		process_list.running_num,
		process_list.sleeping_num,
		process_list.stoped_num,
		process_list.zombie_num);
	printf("PID\tUSER\t\tPRI\tS\tCPU%%\tMEM%%\tTIME+\tCommand\n");
	tmp = process_list.process;
	while (tmp)
	{
		pw = getpwuid(tmp->uid);
		if(pw)
		{
			printf("%d\t%s\t\t%d\t%c\t%.2lf\t%.2f\t", 
				tmp->stat_info.pid, 
				pw->pw_name,
				tmp->stat_info.priority,
				tmp->stat_info.state,
				tmp->cpu_usage,
				tmp->mem_usage);
		}
		else
		{
			printf("%d\t%s\t\t%d\t%c\t%.2lf\t%.2f\t", 
				tmp->stat_info.pid, 
				"NO ONE",
				tmp->stat_info.priority,
				tmp->stat_info.state,
				tmp->cpu_usage,
				tmp->mem_usage);
		}
		print_process_time(tmp->stat_info.starttime);
		printf("%s\n",tmp->stat_info.command);
		tmp = tmp->next;
	}
	
}

