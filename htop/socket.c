#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <sys/time.h>
#include "socket.h"
#include "main.h"
#include "draw.h"

unsigned int operation_mode = 0xFFFFFFFF;
char *server_ip = "127.0.0.1";
int USER_ID = 0;

meminfo memory;
cpuinfo cpu;
process_list_info process_list;


char *populate_message_tail(message_tail *tail_ptr, char *text)
{
	char *stream;

	/*fillup message tail*/
	tail_ptr->id = USER_ID;
	tail_ptr->type = TAIL_TYPE;
	tail_ptr->content = malloc(strlen(text));
	if((tail_ptr->content) == NULL)
	{
		printf("populate message tail failed, because of malloc content failed %s\n",strerror(errno));
		return NULL;
	}
	strcpy(tail_ptr->content, text);
	/* convert to stream */
	stream = calloc(1, sizeof(message_header) + strlen(text));
	memcpy(stream, &(tail_ptr->id), sizeof(int));
	memcpy(stream + offsetof(message_tail,type), &(tail_ptr->type), sizeof(int));
	strcpy(stream + offsetof(message_tail,content), tail_ptr->content);
	memcpy(stream + offsetof(message_tail,reserved), tail_ptr->reserved, sizeof(char)*MESG_RESERVED_SIZE);

	printf("populat message tail success,text %s, %s \n",text,stream + offsetof(message_tail,content));

	return stream;
}

char *populate_message_content(message_content *content_ptr)
{
	static process_info *tmp = NULL;
	char *stream;
	content_ptr->id = USER_ID;
	content_ptr->type = CONTENT_TYPE;
	if(!tmp)
	{
		tmp = process_list.process;
	}

	content_ptr->pro_info= *tmp;
	tmp = tmp->next;
	
	stream = calloc(1, sizeof(message_content));
    memcpy(stream, content_ptr, sizeof(message_content));
	return stream;
}

char *populate_message_header(message_header *header_ptr)
{
    char *stream;

	/*fillup message header*/
    header_ptr->id = USER_ID;
	header_ptr->type = HEADER_TYPE;
    header_ptr->time_val = time(0);
	header_ptr->mem_info = memory;
	header_ptr->cpu_info = cpu;
	header_ptr->proc_size = process_list.size;
	header_ptr->running_num = process_list.running_num;
	header_ptr->sleeping_num = process_list.sleeping_num;
	header_ptr->stoped_num = process_list.stoped_num;
	header_ptr->zombie_num = process_list.zombie_num;

	printf("populate message header success,time %d id %d cpu %s",(unsigned int)header_ptr->time_val,header_ptr->id,header_ptr->cpu_info.model);
    stream = calloc(1, sizeof(message_header));
    memcpy(stream, header_ptr, sizeof(message_header));
   
    return stream;
}

char *decode_message(char *stream,int len)
{
	int type;
	/*type postion must be second byte*/
	message_header header;
	message_content content;
	message_tail tail;
    memcpy(&type, stream + sizeof(int), sizeof(int));
	printf("message type = %d, len = %d\n", type, len);
	if(type == HEADER_TYPE)
	{
		memcpy(&header,stream,sizeof(message_header));
		#if 1
		printf("user id = %d,time = %d,cpu %s\n", header.id, (unsigned int)header.time_val,header.cpu_info.model);
		#endif
	}
	else if(type == CONTENT_TYPE)
	{
		memcpy(&content, stream, sizeof(message_content));
		printf("process pid = %d, command = %s\n",
			content.pro_info.stat_info.pid,
			content.pro_info.stat_info.command);
	}
	else if(type == TAIL_TYPE)
	{
		memcpy(&(tail.id), stream,sizeof(int));
		tail.type = type;
		tail.content = malloc(len - sizeof(int) - sizeof(int) - sizeof(char) * MESG_RESERVED_SIZE);
		memcpy(tail.content, stream + offsetof(message_tail, content), malloc_usable_size(tail.content));
		memcpy(tail.reserved, stream + offsetof(message_tail,reserved), sizeof(char) * MESG_RESERVED_SIZE);

		#if 1
		printf("id %d type %d,len %d\n",tail.id,tail.type,len);
		printf("content %s %s\n",tail.content,stream);
		#endif
	}
   return tail.content;
}

int  print_received_message(int connection_fd)
{
	char buf[RECV_MAX_LEN];
	int recv_len;
	char *mesg;
	
	while((recv_len = recv(connection_fd,buf,RECV_MAX_LEN,0)) > 0)
	{
		mesg = decode_message(buf,recv_len);
		/*
		if(strcmp(mesg, "quit") == 0)
		{
			PINFO("user %d quit connection\n");
			return 1;
		}
		*/
	}
    return 0;
}

int init_server(int domain,int type,int protocal,const struct sockaddr_in *addr, socklen_t alen, int qlen)
{
	int sockfd;
	if((sockfd = socket(domain,type,protocal)) < 0)
    {
        PINFO("init socket failed,%s\n",strerror(errno));
        return -1;
    }
     // bind socket
    if(bind(sockfd,(struct sockaddr*)addr,alen) < 0)
    {
        PINFO("bind socket error,%s\n",strerror(errno));
    }
    // listen
    if(listen(sockfd,qlen) == -1)
    {
        PINFO("listen failed,%s\n",strerror(errno));
    }
    return sockfd;
}
void play_server()
{
    int sockfd,connection_fd;
    int resp_len;
	char *host;
    
    struct sockaddr_in server_addr;

    // init sockaddr_in
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SOCKET_PORT);
    sockfd = init_server(AF_INET,SOCK_STREAM,0,&server_addr,sizeof(server_addr),5);

    //accept and receive mesaage
    while(1)
    {
        if((connection_fd = accept(sockfd,(struct sockaddr *)NULL,NULL)) < 0)
        {
            printf("accept failed\n");
            continue;
        }

        while(! print_received_message(connection_fd))
        {
        	sleep(0.5);
        }
        close(connection_fd);	        
    }
    close(sockfd);
}

int connect_retry(int domain,int type,int protocal,const struct sockaddr_in *addr, socklen_t alen)
{
	int sockfd,i;
	#define MAX_NUM_RETRY 128
	for(i = 0; i < MAX_NUM_RETRY; i <<= 1)
	{
		if((sockfd = socket(domain,type,protocal)) < 0)
	    {
	        printf("init socket failed\n");
	        return -1;
	    }
	    if(connect(sockfd,(const struct sockaddr *)addr,alen) == 0)
	    {
	    	return sockfd;
	    }
	    printf("connect socket error\n");
        close(sockfd);
        sleep(i);
	}
	return -1;
}
int send_message(int sockfd, char *text)
{
	char *stream;
	int send_len = 0, i = 0 ;
	message_header header;
	message_content content;
	message_tail tail;
	
	if((stream = populate_message_header(&header)) == NULL)
    {
        PINFO("populate message header error,%s\n",strerror(errno));
    }
    if((send_len = send(sockfd,stream,malloc_usable_size(stream),0)) < 0)
    {
        PINFO("send message header failed,%s\n",strerror(errno));
    }

	printf("send message header success,len = %d\n", send_len);
	if(stream)
		free(stream);

	while(i < process_list.size)
	{
		if((stream = populate_message_content(&content)) == NULL)
	    {
	        PINFO("populate message content error,%s\n",strerror(errno));
	    }
	    if((send_len = send(sockfd,stream,malloc_usable_size(stream),0)) < 0)
	    {
	        PINFO("send message content failed,%s\n",strerror(errno));
	    }

	    printf("send message content success,len = %d\n", send_len);
		if(stream)
			free(stream);
		i++;
	}
	
	
	if((stream = populate_message_tail(&tail,text)) == NULL)
    {
        PINFO("populate message tail error,%s\n",strerror(errno));
    }
    if((send_len = send(sockfd,stream,malloc_usable_size(stream),0)) < 0)
    {
        PINFO("send message tail failed,%s\n",strerror(errno));
    }

    printf("send message tail success,len = %d\n", send_len);
	if(stream)
		free(stream);
}
void play_client()
{
    
    int recv_len, len;
    int sockfd;
    char buf[RECV_MAX_LEN];

	memset(buf,0,sizeof(buf));

    struct sockaddr_in server_addr;

	//get cpu,memory,process info
	scan(FALSE);
	
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(SOCKET_PORT);

    sockfd = connect_retry(AF_INET,SOCK_STREAM,0,&server_addr,sizeof(server_addr));
    if(sockfd < 0)
    {
    	PINFO("get socket fd fialed,%s\n",strerror(errno));
		exit(EXIT_FAILURE);
	}
	
    printf("please input message:\n");
    while(fgets(buf,sizeof(buf),stdin) != NULL && strcmp(buf,"quit") != 0)
    {
    	len = strlen(buf);
		if(len > 0 && buf[len-1] == '\n')
			buf[len-1] = '\0';
    	send_message(sockfd,buf);
    	printf("please input message:\n");
		memset(buf,0,sizeof(buf));
    }
    if(strcmp(buf,"quit") == 0)
    {
    	send_message(sockfd,buf);
    }
    close(sockfd);
}
