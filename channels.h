#ifndef CHANNELS_H
#define CHANNELS_H

/*includes*/
#include "global.h"

/*structs*/
typedef struct send_arg_t
{
	int32_t channel_id;
	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
}send_arg_t;
typedef struct recv_arg_t
{
	int32_t channel_id;
}recv_arg_t;
typedef struct ret_t
{
	byte_stats_t stats;
}ret_t;

/*functions*/
void *send_channel(void *arg);
void *recv_channel(void *arg);

#endif