#ifndef GLOBAL_H
#define GLOBAL_H

/*includes*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
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

/*macros*/
//the client's maximum segment size
#define SMSS MSS
//intial window- size of congestion window initially
#define IW SMSS * 8
//loss window- size of congestion window after a loss is detected
#define LW SMSS * 1
//system state types
#define SLOWSTART 0
#define CONGESTAVOID 1
#define	FASTRR 2
#define RESET 3

/*structs*/
typedef struct byte_stats_t
{
	int32_t bytes_sent;
	int32_t bytes_dropped;
	int32_t bytes_timedout;
}byte_stats_t;
typedef struct packet_state_t
{
	int32_t id;
	int32_t state;
}packet_state_t;

/*variables*/
const char *MPREQ;
const char *MPOK;
char *err_m;
char *file_buf;
int32_t *sock_hndls;
int32_t num_interfaces;
int32_t file_size;
int32_t transmission_end_sig;
byte_stats_t total_stats;
byte_stats_t *channel_stats;
pthread_mutex_t transmission_end_l;
pthread_mutex_t log_l;
pthread_mutex_t sys_l;


/*functions*/
struct flock *lock_fd(int32_t fd);
int32_t unlock_fd(int32_t fd, struct flock *fl);
int32_t min(int32_t a, int32_t b);
int32_t max(int32_t a, int32_t b);
void write_packet(struct packet pkt, int32_t channel_id, int32_t isSend);


#endif