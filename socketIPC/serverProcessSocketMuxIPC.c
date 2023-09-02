#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_NAME "/tmp/DemoSocket"
#define BUFFER_SIZE 128

#define MAX_CLIENT_SUPPORTED 32

/* An array of file descriptors which the server process is
 * maintaining in order to talk with the connected clients.
 * Master skt FD is also a member of this array */
int monitored_fd_set[MAX_CLIENT_SUPPORTED];

/* Each connected client's intermediate result is maintained
 * in this client array */
int client_result[MAX_CLIENT_SUPPORTED] = {0};

/* Remove all the FDs, if any, from the array */
static void initialize_monitored_fd_set()
{
	int i = 0;
	for(; i < MAX_CLIENT_SUPPORTED; i++)
	{
		monitored_fd_set[i] = -1;
	}
}

/* Add a new FD to the monitored_fd_set array */
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

/* Remove the FD from the monitored_fd_set array */
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

/* Clone all the IDs in monitored_fd_set array into fd_set data
 * structure */
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

/* Get the numerical max value among all FDs which server is
 * monitoring */
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
	}
	return max;
}

int main(int argc, char* argv[])
{
	struct sockaddr_un name;

#if 0
	struct sockaddr_un{
		sa_family_t	sun_family;	/* AF_UNIX */
		char		sun_path[108];	/* pathname */
	};
#endif

	int ret;
	int connection_socket;
	int data_socket;
	int data;
	char buffer[BUFFER_SIZE];
	fd_set readfds;
	int active_socket_fd, i;

	initialize_monitored_fd_set();

	/* In case the program exited inadvertently on the last run:
	 * Remove the socket */
	unlink(SOCKET_NAME);

	/* Create master socket */
	/* SOCK_DGRAM in case of datagram based communication */
	connection_socket = socket(AF_UNIX, SOCK_STREAM, 0);

	if(connection_socket == -1)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	printf("Master socket created\n");

	/* Initialize */
	memset(&name, 0, sizeof(struct sockaddr_un));

	/* Specify the socket credentials */
	name.sun_family = AF_UNIX;
	strncpy(name.sun_path, SOCKET_NAME, sizeof(name.sun_path) - 1);

	/* Bind socket to socket name */
	/* Purpose of bind() system call is that application() dictate
	 * the underlying operating system the criteria of receiving
	 * the data. Here , bind() system call is telling the OS that
	 * if sender procss sends the data destined to socket
	 * "/tmp/DemoSocket", then such data needs to be delivered to
	 * this process (the server process) */
	ret = bind(connection_socket, (const struct sockaddr*) &name, sizeof(struct sockaddr_un));

	if(ret == -1)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}

	printf("bind() call succeed\n");

	/* Prepare for accepting connections. The backlog size is set
	 * to 20
	 * So while one request is being processed other requests can
	 * be waiting */
	ret = listen(connection_socket, 20);
	if(ret == -1)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	/* Add master socket to monitored set of FDs */
	add_to_monitored_fd_set(connection_socket);

	/* This is the main loop for handling connections
	 * All server process usually runs 24x7. Good servers should
	 * always be up and runnning and never go down */
	for(;;)
	{
		refresh_fd_set(&readfds); /* Copy the entire monitored FDs to readfds */
		printf("Waiting on select() sys call\n");

		/* Call the select system call, server process blocks here
		 * Linux OS keeps this process blocked until the connection initiation request
		 * or data requests arrive on any of the file descriptors in the 'readfds' set */
		select(get_max_fd()+1, &readfds, NULL, NULL, NULL);

		if(FD_ISSET(connection_socket, &readfds))
		{
			/* Data arrives on master socket only when new
			 * client connects with the server (that is,
			 * connect() is invoked on client side) */
			printf("New connection received recvd, accept the connection\n");

			data_socket = accept(connection_socket, NULL, NULL);

			if(data_socket == -1)
			{
				perror("accept");
				exit(EXIT_FAILURE);
			}

			printf("Connection accepted for client\n");

			add_to_monitored_fd_set(data_socket);
		}
		else /* Data arrives on some other client FD */
		{
			printf("Data client\n");
			/* Find the client which has sent us the data request */
			i = 0;
			active_socket_fd = -1;
			for(; i < MAX_CLIENT_SUPPORTED; i++)
			{
			        printf("Searching active data client...\n");
				if(FD_ISSET(monitored_fd_set[i], &readfds))
				{
					printf("Active\n");
					active_socket_fd = monitored_fd_set[i];

					/* Prepare the buffer to recv the data */
					memset(buffer, 0, BUFFER_SIZE);

					/* Wait for the next data packet
					 * Server is blocked here. Waiting for the data to arrive from client.
					 * read() is a blocking system call */
					printf("Waiting for data from the client\n");
					ret = read(active_socket_fd, buffer, BUFFER_SIZE);

					if(ret == -1)
					{
						perror("read");
						exit(EXIT_FAILURE);
					}

					/* Add received summand */
					memcpy(&data, buffer, sizeof(int));
					if(data == 0)
					{
						/* Send result */
						memset(buffer, 0, BUFFER_SIZE);
						sprintf(buffer, "Result = %d", client_result[i]);

						printf("Sending final result back to client\n");
						ret = write(active_socket_fd, buffer, BUFFER_SIZE);
						if(ret == -1)
						{
							perror("write");
							exit(EXIT_FAILURE);
						}

						/* Close socket */
						close(active_socket_fd);
						client_result[i] = 0;
						remove_from_monitored_fd_set(active_socket_fd);
						continue; /* Go to select() and block */
					}
					client_result[i] += data;
					continue;
				}
			}
		}
	}/* Go to select() and block */
	
	/* Close master socket */
	close(connection_socket);
	remove_from_monitored_fd_set(connection_socket);
	printf("connection closed...\n");
	
	/* Server should release resources before getting terminated.
	 * unlink the socket */
	unlink(SOCKET_NAME);
	exit(EXIT_SUCCESS);
}
