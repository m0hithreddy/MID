/*
 * MID_socket.c
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#include"MID_socket.h"
#include"MID_structures.h"
#include<errno.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>

struct sockaddr* create_sockaddr_in(char* host,short port,int family)
{
	struct sockaddr_in* addr=(struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));

	bzero(addr,sizeof(struct sockaddr_in));

	addr->sin_family=family;

	addr->sin_port=htons(port);

	if(inet_pton(family,host,&addr->sin_addr)!=1)
		return NULL;

	return (struct sockaddr*)addr;

}

int open_connection(struct socket_info* sock_info,struct sockaddr* addr)
{

	if(sock_info==NULL || addr==NULL)
		return -1;

	int sockfd=socket(sock_info->family,sock_info->type,sock_info->protocol);

	if(sockfd<0)
		return -1;

	for(long i=0;i<sock_info->opts_len;i++)
	{
		if(setsockopt(sockfd,sock_info->sock_opts[i].level,sock_info->sock_opts[i].optname,sock_info->sock_opts[i].optval,sock_info->sock_opts[i].optlen)!=0)
			return -1;
	}

	if(sock_info->address!=NULL)
	{

		struct sockaddr* cliaddr=create_sockaddr_in(sock_info->address,sock_info->port,sock_info->family);

		if(cliaddr==NULL)
			return -1;

		if(bind(sockfd,cliaddr,sizeof(struct sockaddr_in))!=0)
			return -1;
	}

	if(connect(sockfd,addr,sizeof(struct sockaddr_in))!=0)
		return -1;

	return sockfd;
}

struct socket_opt create_socket_opt(int level,int optname,void* optval,socklen_t optlen)
{
	struct socket_opt opt;

	opt.level=level;
	opt.optname=optname;
	opt.optval=(void*)malloc(optlen);
	memcpy(opt.optval,optval,optlen);
	opt.optlen=optlen;

	return opt;
}

long sock_write(int sockfd,struct network_data* n_data)
{
	if(n_data==NULL || n_data->data==NULL || n_data->len<=0)
		return 0;

	int status;
	int written=0;

	while(1)
	{
		status=write(sockfd,n_data->data+written,n_data->len-written);

		if(status==-1 && errno!=EINTR)
		{
			return written;
		}

		if(status!=-1)
			written=written+status;

		if(written>=n_data->len)
			return written;
	}
}

struct network_data* sock_read(int sockfd,long limit)
{
	if(limit<0)
		return NULL;

	struct data_bag *bag=create_data_bag();
	struct network_data *n_data=(struct network_data*)malloc(sizeof(struct network_data));

	n_data->data=(char*)malloc(sizeof(char)*MAX_TRANSACTION_SIZE);

	long counter=0;
	long status=0;

	while(1)
	{
		status=read(sockfd,n_data->data,limit-counter < MAX_TRANSACTION_SIZE ? limit-counter : MAX_TRANSACTION_SIZE);

		if(status==-1 && errno!=EINTR)
			return NULL;

		if(status>0)
		{
			n_data->len=status;
			place_data(bag, n_data);
			counter=counter+status;

			if(counter>=limit)
				break;
		}
		else if(status==0)
			break;
	}

	n_data=flatten_data_bag(bag);

	return n_data;
}

char* resolve_dns(char* hostname)
{
	struct hostent *dnsptr=gethostbyname(hostname);

	if(dnsptr==NULL)
	{
		return NULL;
	}

	if(dnsptr->h_addrtype!=AF_INET) //Confining the application to IPV4
	{
		return NULL;
	}

	char* hostip=(char*)malloc(sizeof(char)*INET_ADDRSTRLEN);

	inet_ntop(dnsptr->h_addrtype,*dnsptr->h_addr_list,hostip,INET_ADDRSTRLEN);

	return hostip;

}







