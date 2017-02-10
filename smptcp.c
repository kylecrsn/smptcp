#include "threads.h"

int main(int argc, char *argv[])
{
	opterr = 0;
	int32_t ret = 0;
	int32_t status;
	int32_t c;
	int32_t serv_sock_fd;
	int32_t num_interfaces;
	int32_t port;
	long num_interfaces_l;
	long port_l;
	err_m = "internal error: ";
	char *n_value = NULL;
	char *h_value = NULL;
	char *p_value = NULL;
	char *f_value = NULL;
	char *usage = "invalid or missing options\nusage: ./mptcp [-n num_interfaces] [-h hostname] [-p port] [-f filename]\n";
	char *end;
	char hostname[INET_ADDRSTRLEN];
	char *filename;
	struct addrinfo hints;
	struct addrinfo *serv_info;
	struct addrinfo *s;
	struct sockaddr_in *serv_addr_name = NULL;
	struct sockaddr_in serv_addr;
	socklen_t serv_addr_len;

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
	if(*end != 0 || errno != 0 || num_interfaces_l < 0 || num_interfaces_l > 1023)
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

	/*
		esatblish initial connection with server
	*/

	//setup server sockaddr_in object
	serv_addr_len = sizeof(serv_addr);
	memset((char *)&serv_addr, 0, serv_addr_len);
	serv_addr.sin_family = AF_INET;
	inet_pton(AF_INET, hostname, &(serv_addr.sin_addr));
	serv_addr.sin_port = htons(port);

	//open a client socket
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




	/*
		recv response for num_interface ports from server
	*/


	/*
		open threads for each port number
	*/


	/*
		detect file transmission completion
	*/


	/*
		resync threads, check for any errors
	*/


	/*
		finish
	*/

	return ret;
}