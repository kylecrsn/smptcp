#include "threads.h"

void *mptcp_thread(void *arg)
{
	int32_t status;
	int32_t serv_sock_fd;
	int32_t channel_id;
	int32_t channel_ct;
	int32_t file_size;
	int32_t port;
	char *filename;
	char *hostname;
	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	socklen_t serv_addr_len;
	socklen_t clnt_addr_len;
	arg_t *args = (arg_t *)arg;
	ret_t *rets = (ret_t *)malloc(sizeof(ret_t));

	/*
		initial configuration
	*/

	//unpack args
	fflush(stdout);
	channel_id = args->channel_id;
	channel_ct = args->channel_ct;
	file_size = args->file_size;
	port = args->port;
	filename = args->filename;
	hostname = args->hostname;
	free(args);

	//initialize rets
	rets->stats.bytes_sent = 0;
	rets->stats.bytes_dropped = 0;
	rets->stats.bytes_resent = 0;

	/*
		esatblish connection with server
	*/

	//setup server sockaddr_in object
	serv_addr_len = sizeof(serv_addr);
	memset((char *)&serv_addr, 0, serv_addr_len);
	serv_addr.sin_family = AF_INET;
	inet_pton(AF_INET, hostname, &(serv_addr.sin_addr));
	serv_addr.sin_port = htons(port);

	//setup client sockaddr_in object
	clnt_addr_len = sizeof(clnt_addr);
	memset((char *)&clnt_addr, 0, clnt_addr_len);
	clnt_addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &(clnt_addr.sin_addr));
	clnt_addr.sin_port = htons(0);

	//open a socket for the data channel
	status = mp_socket(AF_INET, SOCK_MPTCP, IPPROTO_TCP);
	if(status < 0)
	{
		fprintf(stderr, "%s failed to open a socket for data channel %d (errno[%d]: %s)\n", 
				err_m, channel_id, errno, strerror(errno));
		pthread_mutex_unlock(&transmission_end_l);
		pthread_exit((void *)rets);
	}
	serv_sock_fd = status;

	//connect to the server
	status = mp_connect(serv_sock_fd, (struct sockaddr *)&serv_addr, serv_addr_len);
	if(status < 0)
	{
		fprintf(stderr, "%s failed to establish connection with the server on data channel %d (errno[%d]: %s)\n", 
				err_m, channel_id, errno, strerror(errno));
		pthread_mutex_unlock(&transmission_end_l);
		pthread_exit((void *)rets);
	}

	/*
		send data packets to server
	*/

	fprintf(stdout, "Channel %d entering loop...\n", channel_id);
	while(transmission_end_sig == 0)
	{

	}

	pthread_exit((void *)rets);
}