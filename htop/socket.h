#ifndef _SOCKET_H
#define _SOCKET_H

#include "linux_info.h"

#define SOCKET_PORT   5000
#define MAX_CONTENT_LEN     1024
#define RECV_MAX_LEN        (MAX_CONTENT_LEN + 4)

#define HEADER_TYPE		(0x01)
#define CONTENT_TYPE	(0x02)
#define TAIL_TYPE		(0x03)

#define MESG_RESERVED_SIZE (8)
typedef struct message_header
{
    int id;
	int type;
    time_t time_val;
	char resered[MESG_RESERVED_SIZE];
	meminfo mem_info;
	cpuinfo cpu_info;
	unsigned int proc_size;
	unsigned int running_num;
	unsigned int sleeping_num;
	unsigned int stoped_num;
	unsigned int zombie_num;
}__attribute__((packed)) message_header;

typedef struct message_content
{
    int id;
	int type;
	char resered[MESG_RESERVED_SIZE];
	process_info pro_info;
}__attribute__((packed)) message_content;

typedef struct message_tail
{
    int id;
	int type;
	char *content;
    char reserved[MESG_RESERVED_SIZE];
}__attribute__((packed)) message_tail;

void play_client();
void play_server();

#endif