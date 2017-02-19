#include "threads.h"

int main(int argc, char *argv[])
{
	opterr = 0;
	transmission_end_sig = 0;
	int32_t *given_ports;
	int32_t ret = 0;
	int32_t status;
	int32_t i;
	int32_t c;
	int32_t fd;
	int32_t serv_sock_fd;
	int32_t file_size;
	int32_t num_interfaces;
	int32_t port;
	long num_interfaces_l;
	long port_l;
	err_m = "internal error:";
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
	struct flock *fl;
	struct addrinfo hints;
	struct addrinfo *serv_info;
	struct addrinfo *s;
	struct sockaddr_in *serv_addr_name = NULL;
	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	socklen_t serv_addr_len;
	socklen_t clnt_addr_len;
	pthread_t *mptcp_thread_ids;
	struct mptcp_header send_req_header;
	struct mptcp_header *recv_req_header;
	struct packet send_req_packet;
	struct packet *recv_req_packet;
	byte_stats_t *send_stats;
	arg_t *args;
	ret_t *rets;

	/*
		initial configuration
	*/

	pthread_mutex_init(&transmission_end_l, NULL);
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

	//open and lock the file, get its size
	fd = open(filename, O_RDONLY);
	fl = lock_fd(fd);
	if(fl == NULL)
	{
		return 1;
	}
	file_size = get_fsize(filename);
	if(file_size == -1)
	{
		return 1;
	}

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
	write_packet(send_req_packet);

	//send packet to server
	status = mp_send(serv_sock_fd, &send_req_packet, MSS, 0);
	if(status != MSS)
	{
		fprintf(stderr, "%s did not send full packet size to server during interface request phase (bytes: %d/%d)\n", 
				err_m, status, MSS);
		return 1;
	}

	/*
		recv response for num_interface ports from server, create a thread for each port
	*/

	//recv response packet from server
	recv_req_packet = (struct packet *)malloc(sizeof(struct packet));
	recv_req_header = (struct mptcp_header *)malloc(sizeof(struct mptcp_header));
	recv_req_data = (char *)malloc(MSS*sizeof(char));
	recv_req_packet->header = recv_req_header;
	recv_req_packet->data = recv_req_data;
	status = mp_recv(serv_sock_fd, recv_req_packet, MSS, 0);
	write_packet(*recv_req_packet);
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
			given_ports[i] = strtol(recv_req_packet->data, &end, 10);
		}
	}

	//create threads for each interface
	mptcp_sock_hndls = (int32_t *)malloc(num_interfaces*sizeof(int32_t));
	mptcp_thread_ids = (pthread_t *)malloc(num_interfaces*sizeof(pthread_t));
	for(i = 0; i < num_interfaces; i++)
	{
		//spawn data channel threads with new port number
		args = (arg_t *)malloc(sizeof(arg_t));
		args->channel_id = i;
		args->channel_ct = num_interfaces;
		args->file_size = file_size;
		args->port = given_ports[i];
		args->filename = filename;
		args->hostname = hostname;
		fflush(stdout);
		status = pthread_create(&(mptcp_thread_ids[i]), NULL, mptcp_thread, (void *)args);
		if(status)
		{
			fprintf(stderr, "%s failed to spawn a data channel thread (errno[%d]: %s)\n", 
				err_m, errno, strerror(errno));
			
			free(mptcp_sock_hndls);
			free(mptcp_thread_ids);

			return 1;
		}
	}

	//wait for end-of-transmission to be signaled
	pthread_mutex_lock(&transmission_end_l);

	/*
		handle end-of-transmission detection, resync threads, check for any errors
	*/

	//transmission completed succesfully, print statistics after handling threads
	if(transmission_end_sig == 1)
	{
		send_stats = (byte_stats_t *)malloc((num_interfaces+1)*sizeof(byte_stats_t));

		//joining threads, get back data transfer statistics
		for(i = 0; i < num_interfaces; i++)
		{
			fflush(stdout);
			status = pthread_join(mptcp_thread_ids[i], (void **)&rets);
			send_stats[i].bytes_sent = rets->stats.bytes_sent;
			send_stats[num_interfaces].bytes_sent += rets->stats.bytes_sent;
			send_stats[i].bytes_dropped = rets->stats.bytes_dropped;
			send_stats[num_interfaces].bytes_dropped += rets->stats.bytes_dropped;
			send_stats[i].bytes_resent = rets->stats.bytes_resent;
			send_stats[num_interfaces].bytes_resent += rets->stats.bytes_resent;
			free(rets);
		}
	}
	else
	{
		//close sockets and cancel threads
		for(i = 0; i < num_interfaces; i++)
		{
			close(mptcp_sock_hndls[i]);
			fflush(stdout);
			pthread_cancel(mptcp_thread_ids[i]);
		}

		free(mptcp_sock_hndls);
		free(mptcp_thread_ids);

		return 1;
	}
	fprintf(stdout, "\nData Transfer Statistics\n================\n");
	fprintf(stdout, "- Total Bytes Sent: %d\n", send_stats[num_interfaces].bytes_sent);
	fprintf(stdout, "- Total Bytes Dropped: %d\n", send_stats[num_interfaces].bytes_dropped);
	fprintf(stdout, "- Total Bytes Resent: %d\n\n", send_stats[num_interfaces].bytes_resent);
	for(i = 0; i < num_interfaces; i++)
	{
		fprintf(stdout, "Data Channel %d\n=============\n", i);
		fprintf(stdout, "- Bytes Sent: %d\n", send_stats[i].bytes_sent);
		fprintf(stdout, "- Bytes Dropped: %d\n", send_stats[i].bytes_dropped);
		fprintf(stdout, "- Bytes Resent: %d\n\n", send_stats[i].bytes_resent);
	}

	/*
		cleanup and return
	*/

	unlock_fd(fd, fl);
	close(fd);
	free(mptcp_sock_hndls);
	free(mptcp_thread_ids);
	free(send_stats);

	return ret;
}