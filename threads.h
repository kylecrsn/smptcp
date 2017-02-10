#ifndef THREADS_H
#define THREADS_H

/*includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <getopt.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "mptcp.h"

/*structs*/

/*variables*/
char *err_m;

/*functions*/
void *send_thread(void *arg);
void *recv_thread(void *arg);

#endif

/*	fprintf(stdout, "n_val: %d\n", num_interfaces);
	fprintf(stdout, "h_val: %s\n", hostname);
	fprintf(stdout, "p_val: %d\n", port);
	fprintf(stdout, "f_val: %s\n", filename);
*/