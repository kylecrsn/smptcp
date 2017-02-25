#include "channels.h"

void *send_channel(void *arg)
{
	int32_t i;
	int32_t status;
	int32_t channel_id = 0;
	int32_t send_size;
	char msg_buf[SMSS];
	double alpha;
	double max_cwnd_i = 0.0;
	double sum_cwnd_i = 0.0;
	struct sockaddr_in *serv_addr;
	struct sockaddr_in *clnt_addr;
	struct mptcp_header send_req_header;
	struct packet send_req_packet;
	send_arg_t *args = (send_arg_t *)arg;

	/*
		initial configuration
	*/

	//unpack args
	fflush(stdout);
	serv_addr = args->serv_addr;
	clnt_addr = args->clnt_addr;
	free(args);

	//initialize channel_stats
	for(i = 0; i < num_interfaces; i++)
	{
		channel_stats[i].bytes_sent = 0;
		channel_stats[i].bytes_dropped = 0;
		channel_stats[i].bytes_timedout = 0;
	}

	//intitialize total_stats
	total_stats.bytes_sent = 0;
	total_stats.bytes_dropped = 0;
	total_stats.bytes_timedout = 0;

	//the server's segment window
	rwnd = RWIN;
	//limits the total amount of data the can be forward-sent
	cwnd_total = num_interfaces*(num_interfaces*IW);
	//limits the amount of data that can be forward-sent and determines whether to use slow_start or congestion_avoidance in sending data
	channel_wnd = (channel_t *)malloc(num_interfaces*sizeof(channel_t));
	for(i = 0; i < num_interfaces; i++)
	{
		channel_wnd[i].cwnd = num_interfaces*IW;
		channel_wnd[i].ssthresh = rwnd;
	}
	//sequence number (in bytes) for the highest acknowledged packet
	max_ackd_num = 1;
	//sequence number (in bytes) for the highest packet which *could* be sent/forward-sent, controlled by congestion and receiver windows
	max_send_num = 1;
	//sequence number (in bytes) for the most recent packet sent
	last_sent_num = 1;
	//amount of data sent but not acknowledged/accounted for
	flight_size = 0;
	//set to flight_size when a duplicate ACK is first received, used for controlling fast recovery flow
	old_flight_size = 0;
	//number of newly ACKDs bytes since the last time data was sent
	newly_ackd_num = 0;
	//current system state
	system_state = SLOWSTART;
	//keep track of how many duplicate packets are received for retransmission
	dupd_packets_recvd = 0;
	//value limiting the number of duplicate ACKs before intervening
	dupd_packet_limit = 3;
	//set to the most recently receieved ACK value
	this_ack_num = 1;
	//set to the previously received ACK value, used for detecting shift out of duplicated ACK performance
	prev_ack_num = 1;

	/*
		send data packets to server
	*/

	//assemble request packet header
	send_req_header.ack_num = 0;
	send_req_header.total_bytes = file_size;
	send_req_packet.header = &send_req_header;
	send_req_packet.data = msg_buf;

	while(transmission_end_sig == 0)
	{
		pthread_mutex_lock(&sys_l);
		//make sure transmission didn't finish while waiting to obtain the lock
		if(transmission_end_sig != 0)
		{
			pthread_mutex_unlock(&sys_l);
			break;
		}

		//select channel based on the one with the smallest cwnd and update cwnd_total
		cwnd_total = 0;
		for(i = 0; i < num_interfaces; i++)
		{
			cwnd_total += channel_wnd[i].cwnd;
		}

		//cycle through channels to distribute load
		channel_id++;
		if(channel_id == num_interfaces)
		{
			channel_id = 0;
		}

		//compute alpha for any necessary updates to a cwnd
		if(newly_ackd_num > 0)
		{
			max_cwnd_i = 0.0;
			sum_cwnd_i = 0.0;
			for(i = 0; i < num_interfaces; i++)
			{
				if(channel_wnd[i].cwnd > max_cwnd_i)
				{
					max_cwnd_i = (double)channel_wnd[i].cwnd;
				}
				sum_cwnd_i += (double)channel_wnd[i].cwnd/2.5;
			}
			alpha = cwnd_total*(max_cwnd_i/(2.5*2.5))/(sum_cwnd_i*sum_cwnd_i);
		}
		else
		{
			alpha = 0;
		}

		//update system state
		if(system_state != FASTRR)
		{
			if(channel_wnd[channel_id].cwnd < channel_wnd[channel_id].ssthresh)
			{
				system_state = SLOWSTART;
			}
			else
			{
				system_state = CONGESTAVOID;
			}
		}

		//run over state machine to determine whether or not to send a packet and how to update state variables
		switch(system_state)
		{
			case SLOWSTART:
			{
				//increase cwnd by the number of newly received ACKs across channels with MPTCP scaling
				channel_wnd[channel_id].cwnd += min((alpha*newly_ackd_num*SMSS)/cwnd_total, (newly_ackd_num*SMSS)/channel_wnd[channel_id].cwnd);
				newly_ackd_num = 0;
				break;
			}
			case CONGESTAVOID:
			{
				//increase cwnd by the number of newly received ACKs across channels with MPTCP scaling
				if(newly_ackd_num >= channel_wnd[channel_id].cwnd)
				{
					channel_wnd[channel_id].cwnd += min((alpha*newly_ackd_num*SMSS)/cwnd_total, (newly_ackd_num*SMSS)/channel_wnd[channel_id].cwnd);
					newly_ackd_num = 0;
				}
				break;
			}
			case FASTRR:
			{
				//once the dropped packet has been retransmitted, restore the last_sent_num value to temporarily cap off any further transmission
				if(dupd_packets_recvd == dupd_packet_limit+1)
				{
					last_sent_num = max_ackd_num + min(channel_wnd[channel_id].cwnd, rwnd);
				}

				//increase cwnd by the number of newly received ACKs across channels with MPTCP scaling
				channel_wnd[channel_id].cwnd += min((alpha*newly_ackd_num*SMSS)/cwnd_total, (newly_ackd_num*SMSS)/channel_wnd[channel_id].cwnd);
				newly_ackd_num = 0;
				break;
			}
		}

		//update max_send_num to reflect new network conditions
		max_send_num = max_ackd_num + min(channel_wnd[channel_id].cwnd, rwnd);
			
		//printf("last sent: %d\nmaxs: %d\nfile_size: %d\n\n\n", last_sent_num, max_send_num, file_size);
		//send next packet (or retransmit dropped packet) given there's space in the network and the full file hasn't already been sent
		if(last_sent_num < max_send_num && last_sent_num < file_size)
		{
			//set the serv and clnt addresses
			send_req_header.dest_addr = serv_addr[channel_id];
			send_req_header.src_addr = clnt_addr[channel_id];

			//assemble request packet
			send_req_header.seq_num = last_sent_num;
			send_size = min(SMSS, file_size-last_sent_num+1);
			memset(msg_buf, 0, SMSS);
			strncpy(msg_buf, file_buf+last_sent_num-1, send_size);
			log_action(send_req_packet, channel_id, 1);
			log_packet(send_req_packet, channel_id);

			//send packet to server
			status = mp_send(sock_hndls[channel_id], &send_req_packet, send_size, 0);
			if(status != send_size)
			{
				pthread_mutex_lock(&log_l);
				fprintf(stderr, "%s did not send full packet size to server from data channel %d (bytes: %d/%d), please restart transmission\n", 
						err_m, channel_id, status, send_size);
				pthread_mutex_unlock(&log_l);

				pthread_mutex_unlock(&sys_l);
				pthread_mutex_unlock(&transmission_end_l);
				break;
			}

			//update bytes sent values
			channel_stats[channel_id].bytes_sent += send_size;
			total_stats.bytes_sent += send_size;
			flight_size += send_size;

			//if the duplicate limit has been met or a partial ack was received
			if(dupd_packets_recvd == dupd_packet_limit)
			{
				last_sent_num = max_send_num;
			}
			else
			{
				last_sent_num += send_size;
			}
			log_state();
		}
		pthread_mutex_unlock(&sys_l);
	}

	pthread_exit((void *)NULL);
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
	recv_req_data = (char *)malloc(SMSS*sizeof(char));
	recv_req_packet->header = recv_req_header;
	recv_req_packet->data = recv_req_data;

	while(transmission_end_sig == 0)
	{
		//receive packet
		errno = 0;
		(*recv_req_packet->header).ack_num = 0;
		mp_recv(sock_hndls[channel_id], recv_req_packet, 0, 0);

		pthread_mutex_lock(&sys_l);
		//make sure transmission didn't finish while waiting to obtain the lock
		if(transmission_end_sig != 0)
		{
			pthread_mutex_unlock(&sys_l);
			break;
		}

		//if errno is EAGAIN, then the mp_recv function has timed out
		if(errno == EAGAIN)
		{
			//segment loss due to retransmission clock timing out, use multiplicative decrease
			channel_wnd[channel_id].ssthresh = max((flight_size+SMSS)/2, 2*SMSS);
			channel_wnd[channel_id].cwnd = LW;
/*			if(dupd_packets_recvd > dupd_packet_limit)
			{
				last_sent_num = this_ack_num;
			}
			else
			{
				last_sent_num -= SMSS;
			}*/
			last_sent_num = this_ack_num;
			channel_stats[channel_id].bytes_timedout += SMSS;
			total_stats.bytes_timedout += SMSS;

			pthread_mutex_lock(&log_l);
			fprintf(stdout, "[CHANNEL %d]: RECV PACKET %d REQUEST TIMED OUT, RESENDING\n\n", 
				channel_id, (int32_t)ceil((double)max_ackd_num/SMSS));
			pthread_mutex_unlock(&log_l);

			pthread_mutex_unlock(&sys_l);
			continue;
		}
		//if errno is ECONNREFUSED, an error is assumed to have occurred on the server, causing it to prematurely close the connection
		else if(errno == ECONNREFUSED)
		{
			pthread_mutex_lock(&log_l);
			fprintf(stderr, "%s an error occurred on the server causing the connection to close on channel %d, please restart transmission\n", 
				err_m, channel_id);
			pthread_mutex_unlock(&log_l);

			pthread_mutex_unlock(&sys_l);
			pthread_mutex_unlock(&transmission_end_l);
			break;
		}
		log_action(*recv_req_packet, channel_id, 0);
		log_packet(*recv_req_packet, channel_id);

		//catch an ACK of -1 indicating that transmission has completed
		if((*recv_req_packet->header).ack_num == -1)
		{
			transmission_end_sig = 1;
			pthread_mutex_unlock(&sys_l);
			pthread_mutex_unlock(&transmission_end_l);
			break;
		}

		//asign ACK number state variables
		prev_ack_num = this_ack_num;
		this_ack_num = (*recv_req_packet->header).ack_num;
		flight_size -= SMSS;
		if(flight_size < 0)
		{
			flight_size = 0;
		}

		//received acknowledgment for previously unacknowledged data
		if(this_ack_num > prev_ack_num)
		{
			//deflate cwnd and reset any dup* state variables once a new ACK is received after a period of duplicate ACKs
			if(dupd_packets_recvd >= dupd_packet_limit)
			{
				//flight_size is already old_flight_size/2, no need to divide by 2 again
				channel_wnd[channel_id].ssthresh = max(flight_size+SMSS, 2*SMSS);
				channel_wnd[channel_id].cwnd = channel_wnd[channel_id].ssthresh;
				dupd_packets_recvd = 0;
				system_state = RESET;
			}
			newly_ackd_num += SMSS;
			max_ackd_num = this_ack_num;
		}
		//received duplicate acknowledgment
		else
		{
			//first duplicate ACK, decrement one packet from flight size for the dropped packet and store old flight size
			if(dupd_packets_recvd == 0)
			{
				system_state = FASTRR;
				old_flight_size = flight_size+SMSS;
				flight_size -= SMSS;
				channel_stats[channel_id].bytes_dropped += SMSS;
				total_stats.bytes_dropped += SMSS;
			}
			dupd_packets_recvd++;

			//reset last_sent_num when the number of duplicate ACKs hits dupd_packet_limit
			if(dupd_packets_recvd == dupd_packet_limit)
			{
				last_sent_num = this_ack_num;
			}

			//can now begin transmitting unsent packets
			if(flight_size <= (old_flight_size/2)-SMSS)
			{
				newly_ackd_num += SMSS;
			}
		}
		log_state();
		pthread_mutex_unlock(&sys_l);
	}

	//cleanup and exit
	free(recv_req_packet);
	free(recv_req_header);
	free(recv_req_data);

	pthread_exit((void *)NULL);
}