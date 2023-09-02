#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_NAME "/tmp/projectSocket"
#define BUFFER_SIZE 128    /***********/

#define MAX_CLIENT_SUPPORT 32

typedef enum
{
	CREATE,
	UPDATE,
	DELETE
} OPCODE;

typedef struct msg_body
{
	char destination[16];
	char mask;
	char gateway_ip[16];
	char oif[32];
} msg_body_t;

typedef struct sync_msg
{
	OPCODE op_code;
	msg_body_t msg_body;
} sync_msg_t;

int monitored_fd_set[MAX_CLIENT_SUPPORTED];

static void initialize_monitored_fd_set()
{
	int i = 0;
	for(; i < MAX_CLIENT_SUPPORTED; i++)
	{
		monitored_fd_set[i] = -i;
	}
}

static void add_to_monitored_fd_set(int skt_fd)
{
	int i = 0;
	for(; i < MAX_CLIENT_SUPPORTED; i++)
	{
		if(monitored_fd_set[i] != -1)
		{
			continue;
		}
		monitored_fd_set[i] = skt_fd;
		break;
	}
}

static void remove_from_monitored_fd_set(int skt_fd)
{
	int i = 0;
	for(; i < MAX_CLIENT_SUPPORTED; i++)
	{
		if(monitored_fd_set[i] != skt_fd)
		{
			continue;
		}
		monitored_fd_set[i] = -1;
		break;
	}
}

static void refresh_fd_set(fd_set* fd_set_ptr)
{
	FD_ZERO(fd_set_ptr);
	int i = 0;
	for(; i < MAX_CLIENT_SUPPORTED; i++)
	{
		if(monitored_fd_set[i] != -1)
		{
			FD_SET(monitored_fd_set[i], fd_set_ptr);
		}
	}
}

static int get_max_fd()
{
	int i = 0;
	int max = -1;
	for(; i < MAX_CLIENT_SUPPORTED; i++)
	{
		if(monitored_fd_set[i] > max)
		{
			max = monitored_fd_set[i];
		}
		return max;
	} 
}

int main(int argc, char* argv[])
{
}

