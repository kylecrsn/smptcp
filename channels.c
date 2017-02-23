#include "channels.h"

void *send_channel(void *arg)
{
	int32_t i;
	int32_t status;
	int32_t send_size;
	char msg_buf[MSS];
	struct sockaddr_in *serv_addr;
	struct sockaddr_in *clnt_addr;
	struct mptcp_header send_req_header;
	struct packet send_req_packet;
	send_arg_t *args = (send_arg_t *)arg;
	ret_t *rets = (ret_t *)malloc(sizeof(ret_t));



	channel_map = (packet_state_t *)malloc(num_interfaces*sizeof(packet_state_t));
	for(i = 0; i < num_interfaces; i++)
	{
		channel_map[i].id = 0;
		channel_map[i].state = NEW;
	}
	max_ackd_num = 1;
	packets_in_buffer = 0;
	int32_t channel_select;



	/*
		initial configuration
	*/

	//unpack args
	fflush(stdout);
	serv_addr = args->serv_addr;
	clnt_addr = args->clnt_addr;
	free(args);

	//initialize rets
	rets->stats = (byte_stats_t *)malloc(num_interfaces*sizeof(byte_stats_t));
	for(i = 0; i < num_interfaces; i++)
	{
		rets->stats[i].bytes_sent = 0;
		rets->stats[i].bytes_dropped = 0;
		rets->stats[i].bytes_resent = 0;
	}

	//intitialize total_send_stats
	total_send_stats.bytes_sent = 0;
	total_send_stats.bytes_dropped = 0;
	total_send_stats.bytes_resent = 0;

	/*
		send data packets to server
	*/

	//assemble request packet header
	send_req_header.ack_num = 0;
	send_req_header.total_bytes = file_size;
	send_req_packet.header = &send_req_header;
	send_req_packet.data = msg_buf;

	//continue as long as the end_of_transmiison sig hasn't been changed by some recv channel thread
	while(transmission_end_sig == 0)
	{
		//run state machine over channel_map
		pthread_mutex_lock(&packet_map_l);
		if(transmission_end_sig != 0)
		{
			free(channel_map);
			pthread_mutex_unlock(&packet_map_l);
			pthread_exit((void *)rets);
		}

		channel_select = -1;
		packets_in_buffer = 0;
		for(i = 0; i < num_interfaces; i++)
		{
			switch(channel_map[i].state)
			{
				case NEW:
				{
					if(channel_select == -1)
					{
						channel_select = i;
						channel_map[i].state = SENT;
					}
					break;
				}
				case SENT:
				{
					packets_in_buffer++;
					break;
				}
				case TIMEDOUT:
				{
					if(channel_select == -1)
					{
						channel_select = i;
						channel_map[i].state = SENT;
					}
					packets_in_buffer++;
					break;
				}
			}
		}
		if(packets_in_buffer == num_interfaces)
		{
			pthread_mutex_unlock(&packet_map_l);
			continue;
		}

		//set the serv and clnt addresses
		send_req_header.dest_addr = serv_addr[channel_select];
		send_req_header.src_addr = clnt_addr[channel_select];

		//assemble request packet
		send_req_header.seq_num = channel_map[channel_select].id*MSS + 1;
		send_size = min(MSS, file_size-(channel_map[channel_select].id*MSS));
		memset(msg_buf, 0, MSS);
		strncpy(msg_buf, file_buf+(channel_map[channel_select].id*MSS), send_size);
		write_packet(send_req_packet, channel_select, 1);

		//send packet to server
		status = mp_send(sock_hndls[channel_select], &send_req_packet, send_size, 0);
		if(status != send_size)
		{
			fprintf(stderr, "%s did not send full packet size to server from data channel %d (bytes: %d/%d)\n", 
					err_m, channel_select, status, send_size);

			pthread_mutex_unlock(&transmission_end_l);
			pthread_exit((void *)rets);
		}

		//update bytes sent values
		rets->stats[channel_select].bytes_sent += send_size;
		total_send_stats.bytes_sent += send_size;
		pthread_mutex_unlock(&packet_map_l);
	}

	pthread_exit((void *)rets);
}

void *recv_channel(void *arg)
{
	int32_t i;
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
		errno = 0;
		(*recv_req_packet->header).ack_num = 0;
		mp_recv(sock_hndls[channel_id], recv_req_packet, 0, 0);
		if(transmission_end_sig == 1)
		{
			break;
		}
		pthread_mutex_lock(&packet_map_l);

		//mp_recv timed-out as set by setsockopt from the main thread
		if(errno == EAGAIN)
		{
			fprintf(stdout, "[CHANNEL %d]: RECV PACKET %d REQUEST TIMED OUT, RESENDING\n\n", 
				channel_id, (int32_t)ceil((double)max_ackd_num/MSS));
			channel_map[channel_id].state = TIMEDOUT;
			pthread_mutex_unlock(&packet_map_l);
			continue;
		}

		//catch an ACK of -1 indicating the transmission has completed
		if((*recv_req_packet->header).ack_num == -1)
		{
			write_packet(*recv_req_packet, channel_id, 0);
			transmission_end_sig = 1;
			pthread_mutex_unlock(&packet_map_l);
			pthread_mutex_unlock(&transmission_end_l);
			break;
		}

		if((*recv_req_packet->header).ack_num > max_ackd_num)
		{
			switch(channel_map[channel_id].state)
			{
				case SENT:
				{
					max_ackd_num = (*recv_req_packet->header).ack_num;
					for(i = 0; i < num_interfaces; i++)
					{
						channel_map[i].id++;
						channel_map[i].state = NEW;
					}
					break;
				}
				default:
				{
					printf("fuck\n");
				}
			}
			write_packet(*recv_req_packet, channel_id, 0);
		}
		else
		{
			fprintf(stdout, "[CHANNEL %d]: RECV PACKET %d DUPLICATE ACK FROM PREVIOUS REQUEST\n\n", 
				channel_id, max_ackd_num/MSS);
		}
		pthread_mutex_unlock(&packet_map_l);
	}

	free(recv_req_packet);
	free(recv_req_header);
	free(recv_req_data);

	pthread_exit((void *)NULL);
}