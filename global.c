#include "global.h"

struct flock *lock_fd(int32_t fd)
{
	struct flock *fl = (struct flock *)malloc(sizeof(struct flock));
	fl->l_type = F_RDLCK;
	fl->l_whence = SEEK_SET;
	fl->l_start = 0;
	fl->l_len = 0;
	fl->l_pid = getpid();

	//lock the config file for writing
	if(fcntl(fd, F_SETLKW, fl) < 0)
	{
		fprintf(stderr, "%s failed to obtain the read lock on the specified file (errno[%d]: %s)\n", 
				err_m, errno, strerror(errno));
		return NULL;
	}

	return fl;
}

int32_t unlock_fd(int32_t fd, struct flock *fl)
{
	fl->l_type = F_UNLCK;

	//unlock the config file
	if(fcntl(fd, F_SETLK, fl) < 0)
	{
		fprintf(stderr, "%s failed to release the read lock on the specified file (errno[%d]: %s)\n", 
				err_m, errno, strerror(errno));
		return 1;
	}

	free(fl);
	return 0;
}

int32_t min(int32_t a, int32_t b)
{
	if(a <= b)
	{
		return a;
	}
	return b;
}

void write_packet(struct packet pkt, int32_t channel_id, int32_t isSend)
{
	int32_t i;
	char tmp[INET_ADDRSTRLEN];

	//obtain write lock
	pthread_mutex_lock(&write_packet_l);

	//print log header
	if(channel_id > -1)
	{
		fprintf(stdout, "[CHANNEL %d]: ", 
			channel_id);
		if(isSend == 1)
		{
			fprintf(stdout, "SEND PACKET (%d/%d)\n", 
				(int32_t)ceil((double)(*pkt.header).seq_num/MSS), (int32_t)ceil((double)file_size/MSS));
		}
		else
		{
			if((*pkt.header).ack_num == -1)
			{
			fprintf(stdout, "RECV PACKET (%d/%d)\n",
				(*pkt.header).seq_num+1, (int32_t)ceil((double)file_size/MSS));
			}
			else
			{
			fprintf(stdout, "RECV PACKET (%d/%d)\n",
				(*pkt.header).ack_num/MSS, (int32_t)ceil((double)file_size/MSS));
			}
		}
	}
	else
	{
		fprintf(stdout, "[MAIN]: ");
		if(isSend == 1)
		{
			fprintf(stdout, "SEND PACKET (%d/%d)\n", 
				(int32_t)ceil((double)(*pkt.header).seq_num/MSS), (int32_t)ceil((double)(*pkt.header).total_bytes/MSS));
		}
		else
		{
			fprintf(stdout, "RECV PACKET (%d/%d)\n",
				(*pkt.header).seq_num, (int32_t)ceil((double)(*pkt.header).total_bytes/MSS));
		}
	}
	
	//print log body of packet info
	fprintf(stdout, "====================================\n");

	//print out packet map state
	if(channel_id > -1)
	{
		printf("channel_map[");
		for(i = 0; i < num_interfaces-1; i++)
		{
			printf("(%d, %d), ", channel_map[i].id+1, channel_map[i].state);
		}
		printf("(%d, %d)]\n", channel_map[i].id+1, channel_map[i].state);
		fprintf(stdout, "====================================\n");
	}

	inet_ntop((*pkt.header).dest_addr.sin_family, &((*pkt.header).dest_addr.sin_addr), tmp, INET_ADDRSTRLEN);
	fprintf(stdout, "| dest_addr   : %s\n", 
		tmp);
	fprintf(stdout, "| dest_port   : %d\n", 
		ntohs((*pkt.header).dest_addr.sin_port));
	inet_ntop((*pkt.header).src_addr.sin_family, &((*pkt.header).src_addr.sin_addr), tmp, INET_ADDRSTRLEN);
	fprintf(stdout, "| src_addr    : %s\n", 
		tmp);
	fprintf(stdout, "| src_port    : %d\n", 
		ntohs((*pkt.header).src_addr.sin_port));
	fprintf(stdout, "| seq_num     : %d\n", 
		(*pkt.header).seq_num);
	fprintf(stdout, "| ack_num     : %d\n", 
		(*pkt.header).ack_num);
	fprintf(stdout, "| total_bytes : %d\n", 
		(*pkt.header).total_bytes);
	fprintf(stdout, "| data        : %s\n", 
		pkt.data);
	fprintf(stdout, "====================================\n\n");
	
	//release write lock
	pthread_mutex_unlock(&write_packet_l);
}