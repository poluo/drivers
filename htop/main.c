/**********************************************************
 * Author        : poluo
 * Email         : xxx@email.com
 * Create time   : 2017-08-01 09:01
 * Last modified : 2017-08-01 09:01
 * Filename      : main.c
 * Description   : rewrite htop, original in https://github.com/hishamhm/htop
 * *******************************************************/
#include <stdio.h>  //for printf
#include <stdlib.h> //for atioi
#include <stdint.h>
#include <getopt.h> // for getopt_long
#include <string.h>  //strlen
#include <sys/utsname.h> //utsname
#include <sys/time.h> //gettimeofday
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "socket.h"
#include "main.h"
#include "draw.h"
#include "linux_info.h"
#include "log.h"
#include "timer.h"
#include "tools.h"


extern int USER_ID;
extern const char *program_name;

unsigned int hz;	/* Number of ticks per second */
unsigned long long uptime;
uint8_t scan_flag =  0;
uint8_t enable_scan = 0;

int parse_arg(int argc,char *argv[])
{
    int ch;
    char *rvalue = NULL;
    while((ch = getopt(argc,argv,"hcmpadr:")) != -1)
    {
        switch(ch)
        {
            case 'r':
                rvalue = optarg;
                if((strcmp(rvalue,"s") == 0 )||(strcmp(rvalue,"S") == 0))
               	{
               		return SERVER;
               	}
                if((strcmp(rvalue,"c") == 0 )||(strcmp(rvalue,"C") == 0))
                {
                	return CLIENT;
                }
		
                PINFO("argument must be m\\M or s\\S \n");
                break;

            case 'h':
                PINFO("help text\n");
                break;
			
			case 'c':
				scan_flag |= (GET_CPU_INFO);
				break;

			case 'm':
				scan_flag |= (GET_MEM_INFO);
				break;

			case 'p':
				scan_flag |= (GET_PROCESS_INFO);
				break;
				
			case 'a':
            default:
				scan_flag |= (GET_CPU_INFO);
				scan_flag |= (GET_MEM_INFO);
				scan_flag |= (GET_PROCESS_INFO);
                break;
        }
		return OTHERS;
    }   
}

void print_header()
{
	struct utsname header;
	struct tm now_time;
	char date[LOG_DATE_LENGTH];

	uname(&header);
	log_date(date, LOG_DATE_LENGTH);
	printf("%s %s (%s) \t%s \t_%s_\t\n", header.sysname, header.release, header.nodename,
		       date, header.machine);
}

int main(int argc,char *argv[])
{
    int role = parse_arg(argc,argv);
	int s;
	timer_t timerid;
	struct timeval begin, end;
	struct timespec request, remain;
	
	if(role > OTHERS)
	{
		printf("Unsupport parameter\n");
		return 0;
	}
	
	program_name = argv[0];

	print_header();
	get_HZ();
	timerid = init_timer();
	read_uptime(&uptime);
	gettimeofday(&begin,NULL);
	scan(0);
	gettimeofday(&end,NULL);
	debug("Scan time last(us):%llu\n", timeval_to_us(end) - timeval_to_us(begin));
	
	init_timespec_val(&request,1,0);
	
	if(role == CLIENT)
    {
        printf("This is a socket Client\n");
        play_client();
    }
    else if(role == SERVER)
    {
        printf("This is a socket Server\n");
        play_server();
    }
	else if(role == OTHERS)
	{
		while(1)
		{
			s = nanosleep(&request, &remain);
			if((s == -1) && (errno != EINTR))
			{
				printf("errno happened when nanosleep\n");
				exit(1);
			}
			if(s == -1 && ((errno == EINTR)))
			{
				// nanosleep interrupt by signal,maybe not complete
				request = remain;
			}
			if(s == 0)
			{
				//nanosleep complete, redo init request value
				init_timespec_val(&request,1,0);
			}

			if(enable_scan)
			{
				enable_scan = 0;
				gettimeofday(&begin,NULL);
				scan(1);
				gettimeofday(&end,NULL);
				printf("Scan time last(us):%lu\n", timeval_to_us(end) - timeval_to_us(begin));	
			}
		}
	}
	del_timer(timerid);
    return 0;
}
