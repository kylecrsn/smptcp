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

//limits the amount of data that can be forward-sent
int32_t cwnd;
//the server's segment window
int32_t rwnd;
//sequence number (in bytes) for the highest acknowledged packet
int32_t max_ackd_num;
//sequence number (in bytes) for the highest packet which *could* be sent/forward-sent, controlled by congestion and receiver windows
int32_t max_send_num;
//sequence number (in bytes) for the highest packet which has ever been sent
int32_t highest_sent_num;
//sequence number (in bytes) for the most recent packet sent
int32_t last_sent_num;
//amount of data sent but not acknowledged/accounted for
int32_t flight_size;
//set to flight_size when a duplicate ACK is first received, used for controlling fast recovery flow
int32_t old_flight_size;
//determine whether to use slow_start or congestion_avoidance in sending data
int32_t ssthresh;
//number of newly ACKDs bytes since the last time data was sent
int32_t newly_ackd_num;
/*//old round trip time estimation used for triguring retransmission
int32_t old_rtt = 0;
//new round trip time estimation used for triguring retransmission
int32_t new_rtt = 0;
//constant used for recalculating rtt estimation
double alpha_rtt = 0.875;
//sample of rtt
int32_t sample_rtt = 0;*/
//current system state
int32_t system_state;
//keep track of how many duplicate packets are received for retransmission
int32_t dupd_packets_recvd;
//value limiting the number of duplicate ACKs before intervening
int32_t dupd_packet_limit;
//set to the most recently receieved ACK value
int32_t this_ack_num;
//set to the previously received ACK value, used for detecting shift out of duplicated ACK performance
int32_t prev_ack_num;

/*functions*/
struct flock *lock_fd(int32_t fd);
int32_t unlock_fd(int32_t fd, struct flock *fl);
int32_t min(int32_t a, int32_t b);
int32_t max(int32_t a, int32_t b);
void log_action(struct packet pkt, int32_t channel_id, int32_t isSend);
void log_packet(struct packet pkt, int32_t channel_id);
void log_state();
void log_stats(int32_t channel_id, byte_stats_t stats);

#endif