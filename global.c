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

int32_t get_fsize(char *fn)
{
	struct stat st;

	if (stat(fn, &st) == 0)
	{
		return st.st_size;
	}

	fprintf(stderr, "%s encountered an issue while getting the size of the specified file\n", 
				err_m);
	return -1;
}

void write_packet(struct packet pkt)
{
	char tmp[INET_ADDRSTRLEN];

	fprintf(stdout, "<---------- PACKET START ---------->\n");
	inet_ntop((*pkt.header).dest_addr.sin_family, &((*pkt.header).dest_addr.sin_addr), tmp, INET_ADDRSTRLEN);
	fprintf(stdout, "dest_addr   : %s\n", tmp);
	fprintf(stdout, "dest_port   : %d\n", ntohs((*pkt.header).dest_addr.sin_port));
	inet_ntop((*pkt.header).src_addr.sin_family, &((*pkt.header).src_addr.sin_addr), tmp, INET_ADDRSTRLEN);
	fprintf(stdout, "src_addr    : %s\n", tmp);
	fprintf(stdout, "src_port    : %d\n", ntohs((*pkt.header).src_addr.sin_port));
	fprintf(stdout, "seq_num     : %d\n", (*pkt.header).seq_num);
	fprintf(stdout, "ack_num     : %d\n", (*pkt.header).ack_num);
	fprintf(stdout, "total_bytes : %d\n", (*pkt.header).total_bytes);
	fprintf(stdout, "data        : %s\n", pkt.data);
	fprintf(stdout, "<----------- PACKET END ----------->\n\n");
}