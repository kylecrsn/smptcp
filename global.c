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

int32_t max(int32_t a, int32_t b)
{
	if(a >= b)
	{
		return a;
	}
	return b;
}

void log_action(struct packet pkt, int32_t channel_id, int32_t isSend)
{
	char *send_msg = "sending packet (%d/%d) to server\n";
	char *recv_msg = "receving packet (%d/%d) from server\n";

	//obtain log lock
	pthread_mutex_lock(&log_l);

	if(channel_id < 0)
	{
		fprintf(stdout, "[main]: ");
		if(isSend == 1)
		{
			fprintf(stdout, send_msg, 
				(int32_t)ceil((double)(*pkt.header).seq_num/MSS), (int32_t)ceil((double)(*pkt.header).total_bytes/MSS));
		}
		else
		{
			fprintf(stdout, recv_msg,
				(*pkt.header).seq_num, (int32_t)ceil((double)(*pkt.header).total_bytes/MSS));
		}
	}
	else
	{
		fprintf(stdout, "[channel %d]: ", 
			channel_id);
		if(isSend == 1)
		{
			fprintf(stdout, send_msg, 
				(int32_t)ceil((double)(*pkt.header).seq_num/MSS), (int32_t)ceil((double)file_size/MSS));
		}
		else
		{
			if((*pkt.header).ack_num == -1)
			{
			fprintf(stdout, recv_msg,
				(*pkt.header).seq_num+1, (int32_t)ceil((double)file_size/MSS));
			}
			else
			{
			fprintf(stdout, recv_msg,
				(*pkt.header).ack_num/MSS, (int32_t)ceil((double)file_size/MSS));
			}
		}
	}

	//release log lock
	pthread_mutex_unlock(&log_l);
}

void log_packet(struct packet pkt, int32_t channel_id)
{
	char tmp[INET_ADDRSTRLEN];

	//obtain log lock
	pthread_mutex_lock(&log_l);

	fprintf(stdout, "== PACKET CONTENTS =================\n");
	inet_ntop((*pkt.header).dest_addr.sin_family, &((*pkt.header).dest_addr.sin_addr), tmp, INET_ADDRSTRLEN);
	fprintf(stdout, "| dest_addr   : %s\n", tmp);
	fprintf(stdout, "| dest_port   : %d\n", ntohs((*pkt.header).dest_addr.sin_port));
	inet_ntop((*pkt.header).src_addr.sin_family, &((*pkt.header).src_addr.sin_addr), tmp, INET_ADDRSTRLEN);
	fprintf(stdout, "| src_addr    : %s\n", tmp);
	fprintf(stdout, "| src_port    : %d\n", ntohs((*pkt.header).src_addr.sin_port));
	fprintf(stdout, "| seq_num     : %d\n", (*pkt.header).seq_num);
	fprintf(stdout, "| ack_num     : %d\n", (*pkt.header).ack_num);
	fprintf(stdout, "| total_bytes : %d\n", (*pkt.header).total_bytes);
	fprintf(stdout, "| data        : %s\n", pkt.data);
	fprintf(stdout, "====================================\n");
	
	//release log lock
	pthread_mutex_unlock(&log_l);
}

void log_state()
{
	int32_t i;

	//obtain log lock
	pthread_mutex_lock(&log_l);

	fprintf(stdout, "== SYSTEM STATE ====================\n");
	fprintf(stdout, "| cwnd_total              : %d\n", cwnd_total);
	for(i = 0; i < num_interfaces; i++)
	{
		fprintf(stdout, "| channel_wnd[%d].cwnd     : %d\n", i, channel_wnd[i].cwnd);
		fprintf(stdout, "| channel_wnd[%d].ssthresh : %d\n", i, channel_wnd[i].ssthresh);
	}
	fprintf(stdout, "| rwnd                    : %d\n", rwnd);
	fprintf(stdout, "| max_ackd_num            : %d\n", max_ackd_num);
	fprintf(stdout, "| max_send_num            : %d\n", max_send_num);
	fprintf(stdout, "| last_sent_num           : %d\n", last_sent_num);
	fprintf(stdout, "| flight_size             : %d\n", flight_size);
	fprintf(stdout, "| old_flight_size         : %d\n", old_flight_size);
	fprintf(stdout, "| ssthresh                : %d\n", ssthresh);
	fprintf(stdout, "| newly_ackd_num          : %d\n", newly_ackd_num);
	fprintf(stdout, "| system_state            : %d\n", system_state);
	fprintf(stdout, "| dupd_packets_recvd      : %d\n", dupd_packets_recvd);
	fprintf(stdout, "| dupd_packet_limit       : %d\n", dupd_packet_limit);
	fprintf(stdout, "| this_ack_num            : %d\n", this_ack_num);
	fprintf(stdout, "| prev_ack_num            : %d\n", prev_ack_num);
	fprintf(stdout, "====================================\n\n\n");

	//release log lock
	pthread_mutex_unlock(&log_l);
}

void log_stats(int32_t channel_id, byte_stats_t stats)
{
	//obtain log lock
	pthread_mutex_lock(&log_l);

	if(channel_id < 0)
	{
		fprintf(stdout, "\n== OVERALL TRANSMISSION STATISTICS =====================\n");
	}
	else
	{
		fprintf(stdout, "== DATA CHANNEL %d ================", channel_id);
		if(channel_id < 10)
		{
			fprintf(stdout, "=");
		}
		fprintf(stdout, "\n");
	}
	fprintf(stdout, "| Total Bytes Sent       : %d\n", stats.bytes_sent);
	fprintf(stdout, "| Bytes Dropped/Resent   : %d\n", stats.bytes_dropped);
	fprintf(stdout, "| Bytes Timed-Out/Resent : %d\n\n", stats.bytes_timedout);

	//release log lock
	pthread_mutex_unlock(&log_l);
}