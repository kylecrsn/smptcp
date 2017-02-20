#include "channels.h"

void *send_channel(void *arg)
{
	int32_t status;
	int32_t send_size;
	int32_t channel_id;
	char msg_buf[MSS];
	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	struct mptcp_header send_req_header;
	struct packet send_req_packet;
	send_arg_t *args = (send_arg_t *)arg;
	ret_t *rets = (ret_t *)malloc(sizeof(ret_t));

	/*
		initial configuration
	*/

	//unpack args
	fflush(stdout);
	channel_id = args->channel_id;
	serv_addr = args->serv_addr;
	clnt_addr = args->clnt_addr;
	free(args);

	//initialize rets
	rets->stats.bytes_sent = 0;
	rets->stats.bytes_dropped = 0;
	rets->stats.bytes_resent = 0;

	/*
		send data packets to server
	*/

	//assemble request packet header
	send_req_header.dest_addr = serv_addr;
	send_req_header.src_addr = clnt_addr;
	send_req_header.ack_num = 0;
	send_req_header.total_bytes = file_size;
	send_req_packet.header = &send_req_header;
	send_req_packet.data = msg_buf;

	//continue as long as the end_of_transmiison sig hasn't been changed by some thread
	while(transmission_end_sig == 0)
	{
		//make sure not to over-send packets
		pthread_mutex_lock(&bytes_in_transit_l);
		if(bytes_in_transit == RWIN || total_send_stats.bytes_sent == file_size)
		{
			pthread_mutex_unlock(&bytes_in_transit_l);
			continue;
		}

		//assemble request packet
		send_req_header.seq_num = 1 + total_send_stats.bytes_sent;
		send_size = min(MSS, file_size-total_send_stats.bytes_sent);
		memset(msg_buf, 0, MSS);
		strncpy(msg_buf, file_buf+total_send_stats.bytes_sent, send_size);
		write_packet(send_req_packet, channel_id, 1);

		//send packet to server
		status = mp_send(sock_hndls[channel_id], &send_req_packet, send_size, 0);
		if(status != send_size)
		{
			fprintf(stderr, "%s did not send full packet size to server from data channel %d (bytes: %d/%d)\n", 
					err_m, channel_id, status, send_size);

			pthread_mutex_unlock(&transmission_end_l);
			pthread_exit((void *)rets);
		}

		rets->stats.bytes_sent += send_size;
		total_send_stats.bytes_sent += send_size;
		bytes_in_transit += send_size;

		pthread_mutex_unlock(&bytes_in_transit_l);
	}

	pthread_exit((void *)rets);
}

void *recv_channel(void *arg)
{
	int32_t channel_id;
	char *recv_req_data;
	struct mptcp_header *recv_req_header;
	struct packet *recv_req_packet;
	recv_arg_t *args = (recv_arg_t *)arg;

	/*
		initial configuration
	*/

	//unpack args
	fflush(stdout);
	channel_id = args->channel_id;
	free(args);

	/*
		recv ACK packets from server
	*/

	//allocate space for recv packet
	recv_req_packet = (struct packet *)malloc(sizeof(struct packet));
	recv_req_header = (struct mptcp_header *)malloc(sizeof(struct mptcp_header));
	recv_req_data = (char *)malloc(MSS*sizeof(char));
	recv_req_packet->header = recv_req_header;
	recv_req_packet->data = recv_req_data;

	while(transmission_end_sig == 0)
	{
		//receive ACK back
		mp_recv(sock_hndls[channel_id], recv_req_packet, 0, 0);
		write_packet(*recv_req_packet, channel_id, 0);
		if((*recv_req_packet->header).ack_num == -1)
		{
			transmission_end_sig = 1;
			pthread_mutex_unlock(&transmission_end_l);
			break;
		}

		//decrement the number of bytes seen by the window
		pthread_mutex_lock(&bytes_in_transit_l);
		bytes_in_transit -= MSS;
		pthread_mutex_unlock(&bytes_in_transit_l);
	}

	free(recv_req_packet);
	free(recv_req_header);
	free(recv_req_data);

	pthread_exit((void *)NULL);
}