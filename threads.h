#ifndef THREADS_H
#define THREADS_H

/*includes*/
#include "global.h"

/*structs*/
typedef struct arg_t
{
	int32_t channel_id;
	int32_t channel_ct;
	int32_t file_size;
	int32_t port;
	char *filename;
	char *hostname;
}arg_t;
typedef struct ret_t
{
	byte_stats_t stats;
}ret_t;

/*functions*/
void *mptcp_thread(void *arg);

#endif