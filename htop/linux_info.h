#ifndef _LINUX_INFO_H
#define _LINUX_INFO_H
#include <sys/time.h>
#include <unistd.h>


#ifndef PAGE_SIZE
#define PAGE_SIZE ( sysconf(_SC_PAGESIZE) )
#endif
#define ONE_K 1024L
#define PAGE_SIZE_KB ( PAGE_SIZE / ONE_K )
#define COMMADN_MAX_LEN	(20)
typedef struct meminfo
{
   unsigned long long int totalMem;
   unsigned long long int usedMem;
   unsigned long long int freeMem;
   unsigned long long int sharedMem;
   unsigned long long int buffersMem;
   unsigned long long int cachedMem;
   unsigned long long int totalSwap;
   unsigned long long int cachedSwap;
   unsigned long long int usedSwap;
   unsigned long long int freeSwap;
}meminfo;

typedef struct cpuinfo
{
   unsigned long long int usertime;
   unsigned long long int nicetime;
   unsigned long long int systemtime;
   unsigned long long int idletime;
   unsigned long long int ioWait;
   unsigned long long int irq;
   unsigned long long int softIrq;
   unsigned long long int steal;
   unsigned long long int guest;
   unsigned long long int guest_nice;
   unsigned long long int total;
   unsigned long long int last_total;
   unsigned long long int period;
   unsigned int cores;
   unsigned long nr_runing;
   unsigned long nr_threads;
   unsigned long last_pid;
   double lavg1;
   double lavg5;
   double lavg15;
   double utilization;
   char model[50];
}cpuinfo;


typedef struct pid_stats {
  int           pid;                      /** The process id. **/
  unsigned int  comm_len;				  /**thr command length**/
  char          command[COMMADN_MAX_LEN + 1]; 				  /** The filename of the executable **/
  char          state; /** 1 **/          /** R is running, S is sleeping, 
			   D is sleeping in an uninterruptible wait,
			   Z is zombie, T is traced or stopped **/
  int           ppid;                     /** The pid of the parent. **/
  int           pgrp;                     /** The pgrp of the process. **/
  int           session;                  /** The session id of the process. **/
  int           tty;                      /** The tty the process uses **/
  int           tpgid;                    /** (too long) **/
  unsigned int	flags;                    /** The flags of the process. **/
  unsigned int	minflt;                   /** The number of minor faults **/
  unsigned int	cminflt;                  /** The number of minor faults with childs **/
  unsigned int	majflt;                   /** The number of major faults **/
  unsigned int  cmajflt;                  /** The number of major faults with childs **/
  int           utime;                    /** user mode jiffies **/
  int           stime;                    /** kernel mode jiffies **/
  int			cutime;                   /** user mode jiffies with childs **/
  int           cstime;                   /** kernel mode jiffies with childs **/
  int           priority;                 /** the standard nice value, plus fifteen **/
  int 			nice;					  /** The nice value **/
  int  			num_threads;              /** Number of threads in this process (since Linux 2.6)**/
  unsigned int  itrealvalue;              /** The time before the next SIGALRM is sent to the process **/
  int           starttime; /** 20 **/     /** Time the process started after system boot **/
  unsigned int  vsize;                    /** Virtual memory size **/
  unsigned int  rss;                      /** Resident Set Size **/
  unsigned int  rlim;                     /** Current limit in bytes on the rss **/
  unsigned int  startcode;                /** The address above which program text can run **/
  unsigned int	endcode;                  /** The address below which program text can run **/
  unsigned int  startstack;               /** The address of the start of the stack **/
  unsigned int  kstkesp;                  /** The current value of ESP **/
  unsigned int  kstkeip;                 /** The current value of EIP **/
  int			signal;                   /** The bitmap of pending signals **/
  int           blocked; /** 30 **/       /** The bitmap of blocked signals **/
  int           sigignore;                /** The bitmap of ignored signals **/
  int           sigcatch;                 /** The bitmap of catched signals **/
  unsigned int  wchan;  /** 33 **/        /** (too long) **/
		
} pid_stats;
typedef struct pid_io
{
	unsigned long long io_rchar;
   	unsigned long long io_wchar;
   	unsigned long long io_syscr;
   	unsigned long long io_syscw;
	unsigned long long io_read_bytes;
	unsigned long long io_write_bytes;
	unsigned long long io_cancelled_write_bytes;
}pid_io;
typedef struct statm_info
{
	long m_share;
	long m_trs;
	long m_drs;
	long m_lrs;
	long m_dt;
	long m_size;
    long m_resident;
}pid_statm;

typedef struct process_info {
   unsigned long long int time;
   unsigned long long int last_time;
   pid_stats stat_info;
   pid_statm statm_info;
   pid_io io_info;
   double cpu_usage;
   double mem_usage;
   unsigned long uid;
   struct process_info *next;
} process_info;

typedef struct process_list_info
{
    process_info *process;
    unsigned int size;
	unsigned int running_num;
	unsigned int sleeping_num;
	unsigned int stoped_num;
	unsigned int zombie_num;
}process_list_info;

#ifndef MAX_NAME
#define MAX_NAME 128
#endif

#ifndef MAX_READ
#define MAX_READ 1024
#endif

#define STAT			"/proc/stat"
#define UPTIME			"/proc/uptime"
#define PID_STAT	"/proc/%u/stat"
#define PID_STATUS	"/proc/%u/status"
#define PID_IO	"/proc/%u/io"
#define PID_STATM	"/proc/%u/statm"
extern void get_memory_info(meminfo *this);
extern int get_cpu_info(cpuinfo *this);
extern int get_process_list_info(process_list_info *this);
extern void scan(int enable_draw);
extern void read_uptime(unsigned long long *uptime);
#endif