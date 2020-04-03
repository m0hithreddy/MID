/*
 * MID_socket.h
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_SOCKET_H_
#define MID_SOCKET_H_

#define MAX_TRANSACTION_SIZE 65536
#define MAX_TCP_SYN_RETRANSMITS 2

#include"MID_structures.h"
#include<netinet/in.h>
#include<sys/socket.h>

struct socket_info
{
	char* address;
	int port;
	int family;
	int type;
	int protocol;
	struct socket_opt* sock_opts;
	long opts_len;
};

struct socket_opt
{
	int level;
	int optname;
	void* optval;
	socklen_t optlen;
};

struct sockaddr* create_sockaddr_in(char* host,short port,int family);

struct socket_opt create_socket_opt(int level,int optname,void* optval,socklen_t optlen);

int open_connection(struct socket_info* sock_info,struct sockaddr* addr);

long sock_write(int sockfd,struct network_data* n_data);

struct network_data* sock_read(int sockfd,long limit);

char* resolve_dns(char* hostname);


#endif /* MID_SOCKET_H_ */