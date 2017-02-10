#include "threads.h"

int main(int argc, char *argv[])
{
	opterr = 0;
	int32_t option_index = 0;
	int32_t ret = 0;
	int32_t c;
	int32_t num_interfaces_i;
	int32_t port_i;
	long num_interfaces_l;
	long port_l;
	err_msg = "internal error: ";
	char *n_value = NULL;
	char *h_value = NULL;
	char *p_value = NULL;
	char *f_value = NULL;
	char *usage = "invalid or missing options\nusage: ./mptcp [-n num_interfaces] [-h hostname] [-p port] [-f filename]\n";
	char *end;
	struct addrinfo unqualified;
	struct addrinfo *addr_grp;
	struct addrinfo *addr;
	struct sockaddr_in *addr_in;

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
		option_index = 0;
		c = getopt_long(argc, argv, "n:h:p:f:", long_options, &option_index);

		//make sure the end hadn't been reached
		if(c == -1)
			break;

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
	if(ret)
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
	num_interfaces_i = (int32_t)num_interfaces_l;

	//setup unqulaified for searching for the hostname's mapping
	memset(&unqualified, 0, sizeof(unqualified));
	unqualified.ai_family = AF_UNSPEC;
	unqualified.ai_socktype = SOCK_STREAM;

	//get the address info with the unqualified hostname
	ret = getaddrinfo(h_value, "http", &unqualified, &addr_grp);
	if(ret)
	{
		fprintf(stdout, "%s", usage);
		return 1;
	}

	//sort through the address info
	for(addr = addr_grp; addr != NULL; addr = addr->ai_next)
	{
		addr_in = (struct sockaddr_in *)addr->ai_addr;
	}
	freeaddrinfo(addr_grp);

	//make sure the port is specified correctly/within range
	errno = 0;
	port_l = strtol(p_value, &end, 10);
	if(*end != 0 || errno != 0 || port_l < 0 || port_l > 65535)
	{
		fprintf(stdout, "%s", usage);
		return 1;
	}
	port_i = (int32_t)port_l;

	//make sure the filename is valid and exists
	ret = access(f_value, F_OK|R_OK);
	if(ret)
	{
		fprintf(stdout, "%s", usage);
		return 1;
	}

	//esatblish initial connection with server
	//request num_interfaces from server
	//recv num_interfaces port number back
	//open threads for each port number
	//detect file transmission finished
	//resync threads, check for any errors
	//finish

	return ret;
}