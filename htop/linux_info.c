#include <string.h>  //strlen
#include <stdio.h>  //for printf
#include <stdlib.h> //for atioi
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include "main.h"
#include "draw.h"
#include "linux_info.h"

extern cpuinfo cpu;
extern meminfo memory;
extern process_list_info process_list;
extern unsigned char scan_flag;
extern unsigned int hz;
extern unsigned long long uptime;

void get_memory_info(meminfo *this)
{
    FILE* fd = fopen(PROCMEMINFOFILE,"r");
    size_t len = 0;
    ssize_t read;
    if(fd == NULL)
    {
        PINFO("%s open failed\n",PROCMEMINFOFILE);
        return;
    }
    char *buf = NULL;

    while((read = getline(&buf,&len,fd)) != -1)
    {
        #define tryRead(label, variable) (String_startsWith(buf, label) && sscanf(buf + strlen(label), " %32llu kB", variable))
        switch (buf[0]) {
          case 'M':
             if (tryRead("MemTotal:", &this->totalMem)) {}
             else if (tryRead("MemFree:", &this->freeMem)) {}
             else if (tryRead("MemShared:", &this->sharedMem)) {}
             break;
          case 'B':
             if (tryRead("Buffers:", &this->buffersMem)) {}
             break;
          case 'C':
             if (tryRead("Cached:", &this->cachedMem)) {}
             break;
          case 'S':
             switch (buf[1]) {
             case 'w':
                if (tryRead("SwapTotal:", &this->totalSwap)) {}
				else if (tryRead("SwapCached:", &this->cachedSwap)) {}
				else if (tryRead("SwapFree:", &this->freeSwap)) {}
         }
         break;
      }
        #undef tryRead
    }
    fclose(fd);
    if(buf)
    {
        free(buf);
    }
	this->usedSwap = this->totalSwap - this->freeSwap;
	this->usedMem = this->totalMem - this->freeMem;
#if 0
    PDEBUG("MemTotal:\t\t%lld KB\n",this->totalMem);
    PDEBUG("MemFree:\t\t%lld KB\n", this->freeMem);
	PDEBUG("MemUsed:\t\t%lld KB\n", this->usedMem);
    PDEBUG("MemShared:\t%lld KB\n", this->sharedMem);
    PDEBUG("Buffers:\t\t%lld KB\n", this->buffersMem);
    PDEBUG("Cached:\t\t%lld KB\n", this->cachedMem);
    PDEBUG("SwapTotal:\t%lld KB\n", this->totalSwap);
    PDEBUG("SwapFree:\t\t%lld KB\n", this->totalSwap);
#endif
}

int read_cpu_stat_info(cpuinfo *this)
{
	FILE* fd;
	size_t len;
	ssize_t size;
	int cpu_count;
	unsigned long long int use;
	char *tmp = NULL, *buf = NULL;;
	if((fd = fopen(PROCSTATFILE,"r")) == NULL)
	{
        PINFO("%s open failed %s\n",PROCSTATFILE,strerror(errno));
        return -1;
    }

	if((size = getline(&buf,&len,fd)) != -1)
    {
        if(strncmp(buf,"cpu ",4) == 0)
        {
            sscanf(buf, "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu, %16llu", 
                &this->usertime,
                &this->nicetime,
                &this->systemtime,
                &this->idletime,
                &this->ioWait, 
                &this->irq, 
                &this->softIrq, 
                &this->steal, 
                &this->guest,
                &this->guest_nice);
        }
        else
        {
            PINFO("Can not match cpu info\n");
        }
    }
    else
    {
        PINFO("read cpuinfo failed\n");
    }
    fclose(fd);
	
	if(buf)
	{
		free(buf);
	}
	this->last_total = this->total;
	this->total  = this->usertime + this->nicetime + 
		this->systemtime + this->idletime + 
		this->ioWait + this->irq + 
		this->softIrq + this->steal + this->guest + this->guest_nice;
	this->period = (this->total - this->last_total)/this->cores;
}

int read_cpu_cpuinfo(cpuinfo *this)
{
	FILE* fd;
	char line[256];
	char *tmp = NULL;

	if((fd = fopen(PROCCPUINFO, "r")) == NULL)
	{
		PINFO("%s open failed\n",PROCCPUINFO);
		return -1;
	}

	while(fgets(line,sizeof(line),fd) != NULL)
    {	
        if(String_startsWith(line,"model name"))
        {
        	tmp = strchr(line,':');
            memset(this->model,0,sizeof(this->model));
            sscanf(tmp, ": %[^\t\n]",this->model);
        }
		if(String_startsWith(line, "cpu cores"))
		{
			tmp = strchr(line,':');
			sscanf(tmp, ": %d",&this->cores);
		}
    }
    fclose(fd);
	
}

int read_cpu_loadinfo(cpuinfo *this)
{
	FILE* fd;
	char *buf = NULL;
	size_t len;
	ssize_t size;
	if((fd = fopen(PROCLOADAVG,"r")) == NULL)
	{
		PINFO("can not open stat file %s %s\n",PROCLOADAVG,strerror(errno));
        return -1;
	}
	if((size = getline(&buf, &len, fd)) != -1)
	{
		sscanf(buf,"%lf %lf %lf %ld/%ld %ld",
			&this->lavg1,
			&this->lavg5,
			&this->lavg15,
			&this->nr_runing,
			&this->nr_threads,
			&this->last_pid);
	}
	fclose(fd);

	if(buf)
    {
        free(buf);
    }
	
}
int get_cpu_info(cpuinfo *this)
{
	read_cpu_cpuinfo(this);
 	read_cpu_stat_info(this);
	read_cpu_loadinfo(this);
#if 0
    PDEBUG("usertime:%lld \n",this->usertime);
    PDEBUG("nicetime:%lld \n",this->nicetime);
    PDEBUG("systemtime:%lld \n",this->systemtime);
    PDEBUG("idletime:%lld \n",this->idletime);
    PDEBUG("ioWait:%lld \n",this->ioWait);
	PDEBUG("model name:%s\n",this->model);
	PDEBUG("cores:%d\n",this->cores);
	PDEBUG("total:%lld\n",this->total);
#endif
}

static ssize_t xread(int fd, void *buf, size_t count) 
{
  // Read some bytes. Retry on EINTR and when we don't get as many bytes as we requested.
  size_t alreadyRead = 0;
  for(;;) {
     ssize_t res = read(fd, buf, count);
     if (res == -1 && errno == EINTR) continue;
     if (res > 0) {
       buf = ((char*)buf)+res;
       count -= res;
       alreadyRead += res;
     }
     if (res == -1) return -1;
     if (count == 0 || res == 0) return alreadyRead;
  }
}


/*
 ***************************************************************************
 * Read machine uptime, independently of the number of processors.
 * /proc/uptime contains uptime and idle time since machine up
 * OUT:
 * @uptime	Uptime value in jiffies.
 ***************************************************************************
 */
void read_uptime(unsigned long long *uptime)
{
 	FILE *fp;
	char line[128];
	unsigned long up_sec,up_cent;

	if((fp = fopen(UPTIME,"r")) == NULL)
		return;

	if(fgets(line, sizeof(line), fp) == NULL)
	{
		fclose(fp);
		return;
	}

	sscanf(line,"%lu.%lu",&up_sec,&up_cent);
	*uptime = (unsigned long long)up_sec * hz+ 
				(unsigned long long) up_cent * hz / 100;

	fclose(fp);
}

int read_pid_io_info(pid_io *this,unsigned int pid)
{
	char filename[128], line[256];
	FILE *fp;

	sprintf(filename,PID_IO,pid);
	if(!(fp = fopen(filename,"r")))
	{
		PINFO("can not open stat file %s %s\n",filename,strerror(errno));
        return -1;
	}
	
	while(fgets(line,sizeof(line),fp) != NULL)
	{
		if(!strncmp(line, "rchar",5))
		{
			this->io_rchar = strtoull(line+7, NULL, 10);
		}
		else if(!strncmp(line, "read_bytes", 12))
		{
			this->io_read_bytes = strtoull(line+12, NULL, 10);
		}
		else if(!strncmp(line, "whcar", 5))
		{
			this->io_wchar = strtoull(line+7, NULL, 10);
		}
		else if(!strncmp(line, "write_bytes", 13))
		{
			this->io_write_bytes = strtoull(line+13, NULL, 10);
		}
		else if(!strncmp(line, "syscr", 5))
		{
			this->io_syscr = strtoull(line+5, NULL, 10);
		}
		else if(!strncmp(line, "syscw", 5))
		{
			this->io_syscw = strtoull(line+5, NULL, 10);
		}
		else if(!strncmp(line, "cancelled_write_bytes: ", 23)) 
		{
           	this->io_cancelled_write_bytes = strtoull(line+23, NULL, 10);
        }
	}
	fclose(fp);
	return 0;
}

int read_pid_statm_info(pid_statm *this, unsigned int pid)
{
	char filename[128];
	char buf[256];
	int fd, size;

	sprintf(filename,PID_STATM,pid);
	if((fd = open(filename,O_RDONLY)) < 0)
	{
		PINFO("can not open stat file %s %s\n",filename,strerror(errno));
        return -1;
	}
	size = xread(fd, buf, sizeof(buf));
	close(fd);
	if(size < 0)
		return -1;

	buf[size] = '\0';
	sscanf(buf,"%ld %ld %ld %ld %ld %ld %ld",
		&this->m_size,
		&this->m_resident,
		&this->m_share,
		&this->m_trs,
		&this->m_lrs,
		&this->m_drs,
		&this->m_dt);
	return 0;
}
int read_pid_status_info(process_info *this,unsigned int pid)
{
	FILE *fp;
	char filename[128], line[256];

	sprintf(filename, PID_STATUS, pid);
	if(!(fp = fopen(filename,"r")))
	{
		PINFO("can not open status file %s %s\n",filename,strerror(errno));
        return -1;
	}
	
	while(!fgets(line,sizeof(line),fp))
	{
		if(!strncmp(line, "Uid:",4))
		{
			printf("line: %s\n",line);
			sscanf(line + 5, "%lu", &this->uid);
			break;
		}
	}
	fclose(fp);
	return 0;
}
// read stat file
int read_pid_stat_info(process_info *this,unsigned int pid)
{
    int i, size, seconds, fd;
	char filename[128];
	char buf[MAX_READ];
	char *start, *end;

	sprintf(filename,PID_STAT,pid);
    if((fd = open(filename,O_RDONLY)) < 0)
    {
        PINFO("can not open stat file %s %s\n",filename,strerror(errno));
        return -1;
    }
    
    size = xread(fd,buf,MAX_READ);
	close(fd);
	if(size <= 0)
		return -1;
	buf[size] = '\0';
	
    if((start  = strchr(buf,'(') + 1) == NULL)
		return -1;

	if((end  = strchr(buf,')')) == NULL)
		return -1;
	this->stat_info.pid = pid;
    this->stat_info.comm_len= end - start;
	size = this->stat_info.comm_len < COMMADN_MAX_LEN ? this->stat_info.comm_len : COMMADN_MAX_LEN;
    memcpy(this->stat_info.command, start, size);
    this->stat_info.command[size] = '\0';
	this->last_time = (this->stat_info.utime + this->stat_info.stime);
  	sscanf(end + 2, "%c %d %d %d %d %d %u %u %u %u %u %d %d %d %d %d %d %u %u %d %u %u %u %u %u %u %u %u %d %d %d %d %u",
	  		  /*       1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33*/
	  &(this->stat_info.state),
	  &(this->stat_info.ppid),
	  &(this->stat_info.pgrp),
	  &(this->stat_info.session),
	  &(this->stat_info.tty),
	  &(this->stat_info.tpgid),
	  &(this->stat_info.flags),
	  &(this->stat_info.minflt),
	  &(this->stat_info.cminflt),
	  &(this->stat_info.majflt),
	  &(this->stat_info.cmajflt),
	  &(this->stat_info.utime),
	  &(this->stat_info.stime),
	  &(this->stat_info.cutime),
	  &(this->stat_info.cstime),
	  &(this->stat_info.priority),
	  &(this->stat_info.nice),
	  &(this->stat_info.num_threads),
	  &(this->stat_info.itrealvalue),
	  &(this->stat_info.starttime),
	  &(this->stat_info.vsize),
	  &(this->stat_info.rss),
	  &(this->stat_info.rlim),
	  &(this->stat_info.startcode),
	  &(this->stat_info.endcode),
	  &(this->stat_info.startstack),
	  &(this->stat_info.kstkesp),
	  &(this->stat_info.kstkeip),
	  &(this->stat_info.signal),
	  &(this->stat_info.blocked),
	  &(this->stat_info.sigignore),
	  &(this->stat_info.sigcatch),
	  &(this->stat_info.wchan));

	
    this->time = (this->stat_info.utime + this->stat_info.stime);
	this->cpu_usage = (((double)(this->time - this->last_time)) / cpu.period) * 100.0;
	//printf("pid %d %d %d %d\n",pid,this->time,this->last_time,cpu.period);

#if 0
    PDEBUG("pid %d\n",this->stat_info.pid);
    PDEBUG("command %s\n",this->stat_info.command);
    PDEBUG("state %c\n",this->stat_info.state);
	PDEBUG("cpu usage %lf\n",this->cpu_usage);
#endif
	return 0;
}

void get_process_ptr(process_list_info *this,int pid,process_info **proc_ptr)
{
	int i;
	process_info *tmp;

	if(! this->process)
	{
		this->process = calloc(1, sizeof(process_info));
		*proc_ptr = this->process;
		this->process->next = NULL;
		this->size = 1;
		return;
	}
	else
	{
		tmp = this->process;
		while(tmp->stat_info.pid != pid && tmp->next != NULL)
		{
			tmp = tmp->next;
		}
		if(tmp->stat_info.pid == pid)
		{
			*proc_ptr = tmp;
		}
		else
		{
			this->size ++;
			tmp->next = calloc(1,sizeof(process_info));
			tmp = tmp->next;
			tmp->next = NULL;
			*proc_ptr = tmp;
			return;
		}
		
	}
}

int get_process_list_info(process_list_info *this)
{
    DIR *dir;
    struct dirent *entry;
	process_info *proc;
	int count = 0;

    dir = opendir(PROCDIR);
    if(!dir) 
    {
        PINFO("can not open /proc,error %s\n",strerror(errno));
		return -1;
    }

	this->running_num = 0;
	this->sleeping_num = 0;
	this->stoped_num = 0;
	this->zombie_num = 0;
    while((entry = readdir(dir)) != NULL)
    {
        char *name = entry->d_name;
	
		if(name[0] < '0' || name[0] > '9')
            continue;
        int pid = atoi(name);
		
		get_process_ptr(this,  pid, &proc);
		
		if(read_pid_stat_info(proc,pid) < 0)
			PINFO("read pid %d stat file error\n",pid);

		if(read_pid_status_info(proc,pid) < 0)
			PINFO("read pid %d status file error\n",pid);

		if(read_pid_statm_info(&(proc->statm_info),pid) < 0)
			PINFO("read pid %d statm file error\n",pid);

		if(read_pid_io_info(&(proc->io_info),pid) < 0)
			PINFO("read pid %d io file error\n",pid);
		proc->mem_usage = ((double)(proc->statm_info.m_resident * PAGE_SIZE_KB) /  memory.totalMem) * 100.0;
		if(proc->stat_info.state == 'R')
			this->running_num ++;
		else if(proc->stat_info.state == 'S')
			this->sleeping_num++;
		else if(proc->stat_info.state == 'T')
			this->stoped_num++;
		else if(proc->stat_info.state == 'Z')
			this->zombie_num++;
		proc = NULL;
		count++;
		if(count > 10)
			break;
    }
    closedir(dir);
	return 0;
}
void scan(int enable_draw)
{
	
	read_uptime(&uptime);
	get_cpu_info(&cpu);
	get_memory_info(&memory);
	get_process_list_info(&process_list);
	if(enable_draw)
	{
		draw_memory();
		draw_cpu();
		draw_process();
	}
}

