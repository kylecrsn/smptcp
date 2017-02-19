#ifndef GLOBAL_H
#define GLOBAL_H

/*includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "mptcp.h"

/*structs*/
typedef struct byte_stats_t
{
	int32_t bytes_sent;
	int32_t bytes_dropped;
	int32_t bytes_resent;
}byte_stats_t;

/*variables*/
const char *MPREQ;
const char *MPOK;
int32_t *mptcp_sock_hndls;
int32_t transmission_end_sig;
char *err_m;
pthread_mutex_t transmission_end_l;

/*functions*/
struct flock *lock_fd(int32_t fd);
int32_t unlock_fd(int32_t fd, struct flock *fl);
int32_t get_fsize(char *fn);
void write_packet(struct packet pkt);

#endif