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
	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	socklen_t serv_addr_len;
	socklen_t clnt_addr_len;
	arg_t *args = (arg_t *)args;
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
	serv_addr = args->serv_addr;
	clnt_addr = args->clnt_addr;
	serv_addr_len = args->serv_addr_len;
	clnt_addr_len = args->clnt_addr_len;
	free(args);

	//initialize rets
	rets->stats.bytes_sent = 0;
	rets->stats.bytes_dropped = 0;
	rets->stats.bytes_resent = 0;

	/*
		esatblish connection with server
	*/

	//adjust server sockaddr_in object to point to the new port
	serv_addr.sin_port = htons(port);

	//open a socket for the data channel
	status = mp_socket(AF_INET, SOCK_MPTCP, IPPROTO_TCP);
	if(status < 0)
	{
		fprintf(stderr, "%s failed to open a socket for data channel %d (errno[%d]: %s)\n", 
				err_m, channel_id, errno, strerror(errno));
		pthread_mutex_unlock(&transfer_l);
		pthread_exit((void *)rets);
	}
	serv_sock_fd = status;

	//connect to the server
	status = mp_connect(serv_sock_fd, (struct sockaddr *)&serv_addr, serv_addr_len);
	if(status < 0)
	{
		fprintf(stderr, "%s failed to establish connection with the server on data channel %d (errno[%d]: %s)\n", 
				err_m, channel_id, errno, strerror(errno));
		pthread_mutex_unlock(&transfer_l);
		pthread_exit((void *)rets);
	}

	/*
		send data packets to server
	*/

	while(transfer_sig == 0)
	{

	}

	pthread_exit((void *)rets);
}