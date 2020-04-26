/*
 * MID_socket.c
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#include"MID_socket.h"
#include"MID_structures.h"
#include"MID_functions.h"
#include"MID_http.h"
#include"MID.h"
#include<errno.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<netinet/tcp.h>

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

struct socket_info* create_socket_info(char* if_name,char* if_addr)
{
	struct socket_info* sock_info=(struct socket_info*)malloc(sizeof(struct socket_info));

	if(if_name!=NULL && if_addr!=NULL)
		sock_info->address=if_addr;
	else
		sock_info->address=NULL;

	sock_info->port=0;
	sock_info->family=DEFAULT_HTTP_SOCKET_FAMILY;
	sock_info->type=DEFAULT_HTTP_SOCKET_TYPE;
	sock_info->protocol=DEFAULT_HTTP_SOCKET_PROTOCOL;

	sock_info->sock_opts=(struct socket_opt*)malloc(sizeof(struct socket_opt)*((if_addr==NULL || if_name==NULL)? 1 : 2));

	sock_info->sock_opts[0].level=IPPROTO_TCP;
	sock_info->sock_opts[0].optname=TCP_SYNCNT;
	sock_info->sock_opts[0].optval=&args->max_tcp_syn_retransmits;
	sock_info->sock_opts[0].optlen=sizeof(int);

	if(if_name!=NULL && if_addr!=NULL)
	{
		sock_info->sock_opts[1].level=SOL_SOCKET;
		sock_info->sock_opts[1].optname=SO_BINDTODEVICE;
		sock_info->sock_opts[1].optval=if_name;
		sock_info->sock_opts[1].optlen=strlen(if_name);

		sock_info->opts_len=2;
	}
	else
		sock_info->opts_len=1;

	return sock_info;

}
long sock_write(int sockfd,struct mid_data* n_data)
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

struct mid_data* sock_read(int sockfd,long limit)
{
	if(limit<0)
		return NULL;

	struct mid_bag *bag=create_mid_bag();
	struct mid_data *n_data=(struct mid_data*)malloc(sizeof(struct mid_data));

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
			place_mid_data(bag, n_data);
			counter=counter+status;

			if(counter>=limit)
				break;
		}
		else if(status==0)
			break;
	}

	n_data=flatten_mid_bag(bag);

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

char** resolve_dns_mirros(char* hostname,long* n_mirrors)
{
	struct hostent *dnsptr=gethostbyname(hostname);

	if(dnsptr==NULL)
	{
		*n_mirrors=0;
		return NULL;
	}

	if(dnsptr->h_addrtype!=AF_INET) //Confining the application to IPV4
	{
		*n_mirrors=0;
		return NULL;
	}

	char** n_hosts=dnsptr->h_addr_list;

	char** hostip_ptr;
	struct mid_data ip_data;
	struct mid_bag* hostsbag=create_mid_bag();

	long mirrors_count=0;

	for( ; *n_hosts!=NULL && mirrors_count<*n_mirrors; n_hosts++)
	{
		hostip_ptr=(char**)malloc(sizeof(char*));
		*hostip_ptr=(char*)malloc(sizeof(char)*INET_ADDRSTRLEN);

		inet_ntop(dnsptr->h_addrtype,*n_hosts,*hostip_ptr,INET_ADDRSTRLEN);

		ip_data.data=hostip_ptr;
		ip_data.len=sizeof(char*);

		place_mid_data(hostsbag,&ip_data);
		mirrors_count++;
	}

	*n_mirrors=mirrors_count;

	if(mirrors_count==0)
		return NULL;
	else
		return ((char**)(flatten_mid_bag(hostsbag)->data));

}
