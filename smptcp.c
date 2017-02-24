#include "channels.h"

int main(int argc, char *argv[])
{
	opterr = 0;
	transmission_end_sig = 0;
	int32_t *given_ports;
	int32_t ret = 0;
	int32_t main_channel_id = -1;
	int32_t status;
	int32_t i;
	int32_t c;
	int32_t fd;
	int32_t serv_sock_fd;
	int32_t port;
	long num_interfaces_l;
	long port_l;
	err_m = "[ERR]:";
	MPREQ = "MPREQ";
	MPOK = "MPOK";
	char *n_value = NULL;
	char *h_value = NULL;
	char *p_value = NULL;
	char *f_value = NULL;
	char *data_stub = NULL;
	char *toks = " :";
	char *usage = "invalid or missing options\nusage: ./mptcp [-n num_interfaces] [-h hostname] [-p port] [-f filename]\n";
	char *end;
	char *filename;
	char *recv_req_data;
	char hostname[INET_ADDRSTRLEN];
	char msg_buf[MSS];
	FILE *fp;
	struct timeval recv_timeout;
	struct flock *fl;
	struct addrinfo hints;
	struct addrinfo *serv_info;
	struct addrinfo *s;
	struct sockaddr_in *serv_addr_name = NULL;
	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	struct sockaddr_in *channel_serv_addr;
	struct sockaddr_in *channel_clnt_addr;
	socklen_t serv_addr_len;
	socklen_t clnt_addr_len;
	socklen_t *channel_serv_addr_len;
	socklen_t *channel_clnt_addr_len;
	pthread_t send_channel_id;
	pthread_t *recv_channel_ids;
	struct mptcp_header send_req_header;
	struct mptcp_header *recv_req_header;
	struct packet send_req_packet;
	struct packet *recv_req_packet;
	send_arg_t *send_args;
	recv_arg_t *recv_args;

	/*
		initial configuration
	*/

	pthread_mutex_init(&transmission_end_l, NULL);
	pthread_mutex_init(&log_l, NULL);
	pthread_mutex_init(&sys_l, NULL);
	pthread_mutex_lock(&transmission_end_l);

	/*
		parse through user input
	*/

	//begin command-line parsing
	while(1)
	{
		//setup the options array
		static struct option long_options[] = 
			{
				{"num_interfaces",	required_argument,	0,	'n'},
				{"hostname",		required_argument,	0,	'h'},
				{"port",			required_argument,	0,	'p'},
				{"filename",		required_argument,	0,	'f'},
				{0, 0, 0, 0}
			};

		//initialize the index and c
		int option_index = 0;
		c = getopt_long(argc, argv, "n:h:p:f:", long_options, &option_index);

		//make sure the end hadn't been reached
		if(c == -1)
		{
			break;
		}

		//cycle through the arguments
		switch(c)
		{
			case 'n':
			{
				n_value = optarg;
				break;
			}
			case 'h':
			{
				h_value = optarg;
				break;
			}
			case 'p':
			{
				p_value = optarg;
				break;
			}
			case 'f':
			{
				f_value = optarg;
				break;
			}
			case '?':
			{
				ret = 1;
				break;
			}
			default:
			{
				ret = 1;
				break;
			}
		}
	}

	//post-parsing error handling
	if(ret || n_value == NULL || h_value == NULL || p_value == NULL || f_value == NULL)
	{
		fprintf(stdout, "%s", usage);
		return 1;
	}

	//make sure the num_interfaces is specified correctly/within range
	errno = 0;
	num_interfaces_l = strtol(n_value, &end, 10);
	if(*end != 0 || errno != 0 || num_interfaces_l < 1 || num_interfaces_l > 16)
	{
		fprintf(stdout, "%s", usage);
		return 1;
	}
	num_interfaces = (int32_t)num_interfaces_l;

	//setup unqulaified for searching for the hostname's mapping
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	//get the address info with the unqualified hostname
	ret = getaddrinfo(h_value, "http", &hints, &serv_info);
	if(ret)
	{
		fprintf(stdout, "%s", usage);
		return 1;
	}

	//sort through the address info
	for(s = serv_info; s != NULL; s = s->ai_next)
	{
		//ignore trailing null ip of common public-facing hostnames
		if(strcmp(inet_ntop(AF_INET, &(((struct sockaddr_in *)s->ai_addr)->sin_addr), hostname, INET_ADDRSTRLEN), "0.0.0.0") == 0)
		{
			break;
		}
		serv_addr_name = (struct sockaddr_in *)s->ai_addr;
	}
	if(serv_addr_name == NULL)
	{
		fprintf(stdout, "%s", usage);
		return 1;
	}
	inet_ntop(AF_INET, &(serv_addr_name->sin_addr), hostname, INET_ADDRSTRLEN);
	freeaddrinfo(serv_info);

	//make sure the port is specified correctly/within range
	errno = 0;
	port_l = strtol(p_value, &end, 10);
	if(*end != 0 || errno != 0 || port_l < 0 || port_l > 65535)
	{
		fprintf(stdout, "%s", usage);
		return 1;
	}
	port = (int32_t)port_l;

	//make sure the filename is valid and exists
	ret = access(f_value, F_OK|R_OK);
	if(ret)
	{
		fprintf(stdout, "%s", usage);
		return 1;
	}
	filename = f_value;

	//open file, place read-lock on file, get its size, read its contents into a global buffer
	fp = fopen(filename, "r");
	fd = fileno(fp);
	fl = lock_fd(fd);
	if(fl == NULL)
	{
		fprintf(stderr, "%s failed to obtain a read-lock on the file specified\n", 
			err_m);
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	rewind(fp);
	file_buf = (char *)malloc(file_size*sizeof(char));
	fread(file_buf, file_size, sizeof(char), fp);

	/*
		esatblish initial connection with server
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

	//open a socket
	status = mp_socket(AF_INET, SOCK_MPTCP, IPPROTO_TCP);
	if(status < 0)
	{
		fprintf(stderr, "%s failed to open an intial server socket (errno[%d]: %s)\n", 
				err_m, errno, strerror(errno));
		return 1;
	}
	serv_sock_fd = status;

	//connect to the server
	status = mp_connect(serv_sock_fd, (struct sockaddr *)&serv_addr, serv_addr_len);
	if(status < 0)
	{
		fprintf(stderr, "%s failed to establish intial connection with the server (errno[%d]: %s)\n", 
				err_m, errno, strerror(errno));
		return 1;
	}

	/*
		send request for num_interface ports to server
	*/

	//assemble request packet header
	send_req_header.dest_addr = serv_addr;
	send_req_header.src_addr = clnt_addr;
	send_req_header.seq_num = 1;
	send_req_header.ack_num = 0;

	//assemble request packet
	memset(msg_buf, 0, MSS);
	send_req_header.total_bytes = sprintf(msg_buf, "%s %d", MPREQ, num_interfaces);
	send_req_packet.header = &send_req_header;
	send_req_packet.data = msg_buf;
	log_action(send_req_packet, main_channel_id, 1);
	log_packet(send_req_packet, main_channel_id);

	//send packet to server
	status = mp_send(serv_sock_fd, &send_req_packet, MSS, 0);
	if(status != MSS)
	{
		fprintf(stderr, "%s did not send full packet size to server during interface request phase (bytes: %d/%d)\n", 
				err_m, status, MSS);
		return 1;
	}

	/*
		recv response for num_interface ports from server
	*/

	//setup recv packet memory
	recv_req_packet = (struct packet *)malloc(sizeof(struct packet));
	recv_req_header = (struct mptcp_header *)malloc(sizeof(struct mptcp_header));
	recv_req_data = (char *)malloc(MSS*sizeof(char));
	recv_req_packet->header = recv_req_header;
	recv_req_packet->data = recv_req_data;

	//recv response packet from server
	status = mp_recv(serv_sock_fd, recv_req_packet, MSS, 0);
	log_action(*recv_req_packet, main_channel_id, 0);
	log_packet(*recv_req_packet, main_channel_id);
	if((*recv_req_packet->header).ack_num == -1)
	{
		fprintf(stderr, "%s did not receive full packet size from server during interface request phase (bytes: %d/%d)\n", 
			err_m, status, MSS);

		free(recv_req_packet);
		free(recv_req_header);
		free(recv_req_data);

		return 1;
	}

	//parse response data into an array of port values
	data_stub = strtok(recv_req_packet->data, toks);
	if(strcmp(data_stub, MPOK) != 0)
	{
		fprintf(stderr, "%s did not receive a MPOK response packet from server during interface request phase (type: %s)\n", 
			err_m, data_stub);

		free(recv_req_packet);
		free(recv_req_header);
		free(recv_req_data);

		return 1;
	}
	given_ports = (int32_t *)malloc(num_interfaces*sizeof(int32_t));
	for(i = 0; data_stub != NULL; i++)
	{
		data_stub = strtok(NULL, toks);
		if(data_stub != NULL)
		{
			given_ports[i] = strtol(data_stub, &end, 10);
		}
	}

	/*
		spawn master send channel and a pool of recv channels for each port
	*/

	//timeout value for receiving ACKs, 1 sec == 1000000 usec
	recv_timeout.tv_sec = 20;
	recv_timeout.tv_usec = 0;

	//open sockets for each interface and spawn related recv channel thread
	sock_hndls = (int32_t *)malloc(num_interfaces*sizeof(int32_t));
	recv_channel_ids = (pthread_t *)malloc(num_interfaces*sizeof(pthread_t));
	channel_serv_addr = (struct sockaddr_in *)malloc(num_interfaces*sizeof(struct sockaddr_in));
	channel_clnt_addr = (struct sockaddr_in *)malloc(num_interfaces*sizeof(struct sockaddr_in));
	channel_serv_addr_len = (socklen_t *)malloc(num_interfaces*sizeof(socklen_t));
	channel_clnt_addr_len = (socklen_t *)malloc(num_interfaces*sizeof(socklen_t));
	channel_stats = (byte_stats_t *)malloc(num_interfaces*sizeof(byte_stats_t));
	for(i = 0; i < num_interfaces; i++)
	{
		//setup channel server sockaddr_in object
		channel_serv_addr_len[i] = sizeof(channel_serv_addr[i]);
		memset((char *)&channel_serv_addr[i], 0, channel_serv_addr_len[i]);
		channel_serv_addr[i].sin_family = AF_INET;
		inet_pton(AF_INET, hostname, &(channel_serv_addr[i].sin_addr));
		channel_serv_addr[i].sin_port = htons(given_ports[i]);

		//setup channel client sockaddr_in object
		channel_clnt_addr_len[i] = sizeof(channel_clnt_addr[i]);
		memset((char *)&channel_clnt_addr[i], 0, channel_clnt_addr_len[i]);
		channel_clnt_addr[i].sin_family = AF_INET;
		inet_pton(AF_INET, "127.0.0.1", &(channel_clnt_addr[i].sin_addr));
		channel_clnt_addr[i].sin_port = htons(0);

		//open a socket for the data channel
		status = mp_socket(AF_INET, SOCK_MPTCP, IPPROTO_TCP);
		if(status < 0)
		{
			fprintf(stderr, "%s failed to open a socket for data channel %d (errno[%d]: %s)\n", 
					err_m, i, errno, strerror(errno));

			free(sock_hndls);
			free(recv_channel_ids);
			free(channel_serv_addr);
			free(channel_clnt_addr);
			free(channel_serv_addr_len);
			free(channel_clnt_addr_len);
			free(channel_stats);

			return 1;
		}
		sock_hndls[i] = status;

		//setup socket timeout for recv channel
		setsockopt(sock_hndls[i], SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)); 

		//connect to the server
		status = mp_connect(sock_hndls[i], (struct sockaddr *)&channel_serv_addr[i], channel_serv_addr_len[i]);
		if(status < 0)
		{
			fprintf(stderr, "%s failed to establish connection with the server on data channel %d (errno[%d]: %s)\n", 
					err_m, i, errno, strerror(errno));
			
			free(sock_hndls);
			free(recv_channel_ids);
			free(channel_serv_addr);
			free(channel_clnt_addr);
			free(channel_serv_addr_len);
			free(channel_clnt_addr_len);
			free(channel_stats);

			return 1;
		}

		//spawn recv channel thread for each interface
		recv_args = (recv_arg_t *)malloc(sizeof(recv_arg_t));
		recv_args->channel_id = i;
		status = pthread_create(&(recv_channel_ids[i]), NULL, recv_channel, (void *)recv_args);
		if(status)
		{
			fprintf(stderr, "%s failed to spawn a recv_channel thread (errno[%d]: %s)\n", 
				err_m, errno, strerror(errno));
			
			free(sock_hndls);
			free(recv_channel_ids);
			free(channel_serv_addr);
			free(channel_clnt_addr);
			free(channel_serv_addr_len);
			free(channel_clnt_addr_len);
			free(channel_stats);

			return 1;
		}
	}

	//spawn master send channel thread
	send_args = (send_arg_t *)malloc(sizeof(send_arg_t));
	send_args->serv_addr = channel_serv_addr;
	send_args->clnt_addr = channel_clnt_addr;
	fflush(stdout);
	status = pthread_create(&(send_channel_id), NULL, send_channel, (void *)send_args);
	if(status)
	{
		fprintf(stderr, "%s failed to spawn a send_channel thread (errno[%d]: %s)\n", 
			err_m, errno, strerror(errno));
		
		free(sock_hndls);
		free(recv_channel_ids);
		free(channel_serv_addr);
		free(channel_clnt_addr);
		free(channel_serv_addr_len);
		free(channel_clnt_addr_len);
		free(channel_stats);

		return 1;
	}

	//wait for end-of-transmission to be signaled
	pthread_mutex_lock(&transmission_end_l);

	/*
		handle end-of-transmission detection, resync threads, check for any errors
	*/

	//transmission completed succesfully, print statistics after handling threads
	if(transmission_end_sig == 1)
	{
		//join send channel thread, get back data transfer statistics
		fflush(stdout);
		status = pthread_join(send_channel_id, (void **)NULL);

		//join recv channel threads, rets will be NULL for each
		for(i = 0; i < num_interfaces; i++)
		{
			fflush(stdout);
			pthread_join(recv_channel_ids[i], (void **)NULL);
			close(sock_hndls[i]);
		}
	}
	else
	{
		fprintf(stderr, "%s an error occurred while sending data/receiving ACKs, exiting...\n", 
			err_m);

		//close sockets and cancel threads
		for(i = 0; i < num_interfaces; i++)
		{
			close(sock_hndls[i]);
			fflush(stdout);
			pthread_cancel(recv_channel_ids[i]);
		}
		pthread_cancel(send_channel_id);

		free(sock_hndls);
		free(recv_channel_ids);
		free(channel_serv_addr);
		free(channel_clnt_addr);
		free(channel_serv_addr_len);
		free(channel_clnt_addr_len);
		free(channel_stats);

		return 1;
	}

	//Report data transfer statistics
	log_stats(main_channel_id, total_stats);
	for(i = 0; i < num_interfaces; i++)
	{
		log_stats(i, channel_stats[i]);
	}

	/*
		cleanup and return
	*/

	unlock_fd(fd, fl);
	fclose(fp);
	free(sock_hndls);
	free(recv_channel_ids);
	free(channel_serv_addr);
	free(channel_clnt_addr);
	free(channel_serv_addr_len);
	free(channel_clnt_addr_len);
	free(channel_stats);

	return ret;
}