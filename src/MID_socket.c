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
#include"MID_interfaces.h"
#include"MID_ssl_socket.h"
#include"url_parser.h"
#include<errno.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<netinet/tcp.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/select.h>
#include<sys/time.h>

struct mid_client* create_mid_client(struct mid_interface* mid_if, struct parsed_url* purl)
{
	struct mid_client* mid_cli=calloc(1,sizeof(struct mid_client));

	if(mid_cli == NULL)
		return NULL;

	/* If mid-interface is given then copy name, address, and family */

	if(mid_if != NULL)
	{
		if(mid_if->name != NULL)  // Name.
			mid_cli->if_name = strdup(mid_if->name);

		if(mid_if->address != NULL)  // Address.
			mid_cli->if_addr = strdup(mid_if->address);

		mid_cli->family = mid_if->family;  // Family.
	}

	/* If purl is given then set host-name and port */

	if(purl != NULL)
	{
		if(purl->host != NULL)  // Host-Name
			mid_cli->hostname = strdup(purl->host);

		mid_cli->port= strdup(purl->port == NULL ? \
				( strcmp(purl->scheme,"http") == 0 ?  DEFAULT_HTTP_PORT : DEFAULT_HTTPS_PORT ): \
						purl->port);   // Port.
	}

	/* Use TCP socktes */

	mid_cli->type = MID_DEFAULT_HTTP_SOCKET_TYPE;
	mid_cli->protocol = MID_DEFAULT_HTTP_SOCKET_PROTOCOL;

	/* Misc initializations */

	mid_cli->sockfd = -1;
	mid_cli->hostip = NULL;

	if(!strcmp(purl->scheme,"http"))   // Mid protocol scheme.
		mid_cli->mid_protocol = MID_CONSTANT_APPLICATION_PROTOCOL_HTTP;
	else if(!strcmp(purl->scheme,"https"))
		mid_cli->mid_protocol = MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS;
	else
		mid_cli->mid_protocol = MID_CONSTANT_APPLICATION_PROTOCOL_UNKNOWN;

#ifdef LIBSSL_SANE
	mid_cli->ssl = NULL;
#endif

	return mid_cli;
}

int init_mid_client(struct mid_client* mid_cli)
{
	if(mid_cli == NULL)
		return 0;

	struct addrinfo hints,*result,*rp;

	memset(&hints,0,sizeof(struct addrinfo));   // Set the hints as caller requested
	hints.ai_family = mid_cli->family;
	hints.ai_socktype = mid_cli->type;
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_protocol = mid_cli->protocol;

	int s = getaddrinfo(mid_cli->hostname, mid_cli->port, &hints, &result);   // Call getaddrinfo
	if (s != 0)
		goto init_error;

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		mid_cli->sockfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);

		if(mid_cli->sockfd == -1)
			continue;

		/* Set the socket options */

		if(setsockopt(mid_cli->sockfd, IPPROTO_TCP, TCP_SYNCNT, &args->max_tcp_syn_retransmits, sizeof(int)) != 0)   // TCP SYN retransmit count
			continue;

		if(mid_cli->if_name != NULL && mid_cli->if_addr!=NULL)
		{
			if(setsockopt(mid_cli->sockfd, SOL_SOCKET, SO_BINDTODEVICE, mid_cli->if_name, strlen(mid_cli->if_name)) != 0)   // Set the interface
			continue;

			/* Bind to the interface */

			struct sockaddr_in cli_addr;
			memset(&cli_addr,0,sizeof(struct sockaddr_in));

			cli_addr.sin_family=rp->ai_family;   // Initialize the client side sockaddr_in
			cli_addr.sin_port=htons(0);
			inet_pton(rp->ai_family,mid_cli->if_addr,&cli_addr.sin_addr);

			if(bind(mid_cli->sockfd,(struct sockaddr*)&cli_addr,sizeof(struct sockaddr_in)) != 0)   // call the bind
				continue;
		}

		if(connect(mid_cli->sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
		{
			if(rp->ai_family == AF_INET || rp->ai_family == AF_INET6)   //  If only AF_INET or AF_INET6 then copy
			{
				mid_cli->hostip = (char*)malloc(sizeof(char)*(rp->ai_family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN));
				inet_ntop(rp->ai_family, rp->ai_addr, mid_cli->hostip, rp->ai_family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN);
			}

			break;
		}

		close(mid_cli->sockfd);
	}

	freeaddrinfo(result);

	if (rp == NULL)      // No address succeeded, return ;
		goto init_error;

#ifdef LIBSSL_SANE
	if(mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS)
	{
		if(!init_mid_ssl(mid_cli))
			goto init_error;
	}
#endif

	return 1;

	init_error:

	mid_cli->sockfd = -1;
#ifdef LIBSSL_SANE
	mid_cli->ssl = NULL;
#endif
	mid_cli->hostip = NULL;

	return 0;
}

int close_mid_client(struct mid_client* mid_cli)
{
	if(mid_cli == NULL)
		return 0;

	int return_code = 1;

#ifdef LIBSSL_SANE
	if(mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS && mid_cli->ssl != NULL) {
		if(close_mid_ssl(mid_cli) != 1)
			return_code = 0;
	}
#endif

	if(mid_cli->sockfd >=0 ) {
		if(close(mid_cli->sockfd) != 0)
			return_code = 0;
	}

	return return_code;
}

void free_mid_client(struct mid_client* mid_cli)
{
	if(mid_cli == NULL)
		return;

	/* Free the SSL pointer*/

#ifdef LIBSSL_SANE
	free_mid_ssl(mid_cli);
#endif

	/* Free the pointers */

	free(mid_cli->if_name);
	free(mid_cli->if_addr);
	free(mid_cli->hostname);
	free(mid_cli->port);
	free(mid_cli->hostip);

	/* Free the struct */

	free(mid_cli);

}

int destroy_mid_client(struct mid_client* mid_cli)
{
	int close_status = close_mid_client(mid_cli);
	free_mid_client(mid_cli);

	return close_status;
}

void mid_protocol_quit(struct mid_client* mid_cli)
{
	if(mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTP)    // Is always supported, but quitting as user requested.
		exit(3);
#ifdef LIBSSL_SANE
	else if(mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS)  // Is supported, but quitting as user requested
		exit(3);
#else
	else if(mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS)    // MID not compiled with SSL support
		SSL_quit();
#endif
	else
	{
		mid_err("\nMID: Unsupported protocol encountered. Exiting...\n\n");
		exit(3);
	}
}

int mid_socket_write(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status)
{

	if(mid_cli == NULL || mid_cli->sockfd < 0 || m_data == NULL || \
			m_data->data == NULL || m_data->len <=0 ) { // Invalid input.

		status != NULL ? *status = 0 : 0;
		return MID_ERROR_SOCK_WRITE_INVAL;
	}

	long wr_status = 0, wr_counter = 0;

	fd_set wr_set, tp_set;
	FD_ZERO(&wr_set);
	FD_SET(mid_cli->sockfd, &wr_set);

	int maxfds = mid_cli->sockfd + 1, sl_status = 0;

	for( ;  ; )
	{
		if(mode == MID_MODE_SOCK_WRITE_AUTO_RETRY) {  /* If mode is auto-retry and non blocking sockets are used,
		loop may become busy wait. So use select (works with blocking as well !) */

			tp_set = wr_set;
			sl_status = select(maxfds, NULL, &tp_set, NULL, NULL);

			if(sl_status == -1)
			{
				if(errno == EINTR)
					continue;
				else
				{
					status != NULL ? *status = wr_counter : 0;
					return MID_ERROR_SOCK_WRITE_ERROR;
				}
			}
		}

		wr_status = write(mid_cli->sockfd, m_data->data + wr_counter, \
				m_data->len - wr_counter);    // Commence the write operation.

		if(wr_status < 0)  // If the error condition.
		{
			if(errno == EWOULDBLOCK || errno == EAGAIN || \
					errno == EINTR)  // Probable recoverable errors, act according to caller request.
			{
				if(mode == MID_MODE_SOCK_WRITE_AUTO_RETRY)  // If mode is auto-retry then try again.
					continue;
				else
				{
					status != NULL ? *status = wr_counter : 0;
					return MID_ERROR_SOCK_WRITE_RETRY;
				}
			}
			else   // Fatal Error, report back.
			{
				status != NULL ? *status = wr_counter : 0;
				return MID_ERROR_SOCK_WRITE_ERROR;
			}
		}

		if(wr_status > 0)
			wr_counter = wr_counter + wr_status;

		if(wr_counter < m_data->len)   // If fewer bytes transfered
		{
			if(mode == MID_MODE_SOCK_WRITE_AUTO_RETRY)   // Act as user requested.
				continue;
			else
			{
				status != NULL ? *status = wr_counter : 0;
				return MID_ERROR_SOCK_WRITE_RETRY;
			}
		}

		// Else all bytes are transferred, break;

		break;
	}

	status != NULL ? *status = wr_counter : 0;
	return MID_ERROR_SOCK_WRITE_NONE;
}

int mid_socket_read(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status)
{
	if(mid_cli == NULL || mid_cli->sockfd < 0 || m_data == NULL || \
			m_data->data == NULL || m_data->len <= 0) {  // Invalid input.

		status != NULL ? *status = 0 : 0;
		return MID_ERROR_SOCK_READ_INVAL;
	}

	long rd_status = 0, rd_counter = 0;

	fd_set rd_set, tp_set;
	FD_ZERO(&rd_set);
	FD_SET(mid_cli->sockfd, &rd_set);

	int maxfds = mid_cli->sockfd + 1, sl_status = 0;

	for( ;  ; )
	{
		if(mode == MID_MODE_SOCK_READ_AUTO_RETRY) { /* If mode is auto-retry and non blocking sockets are used,
		loop may become busy wait. So use select (works with blocking as well !) */

			tp_set = rd_set;
			sl_status = select(maxfds, &tp_set, NULL, NULL, NULL);

			if(sl_status == -1)
			{
				if(errno == EINTR)
					continue;
				else
				{
					status != NULL ? *status = rd_counter : 0;
					return MID_ERROR_SOCK_READ_ERROR;
				}
			}
		}

		rd_status = read(mid_cli->sockfd, m_data->data + rd_counter, \
				m_data->len - rd_counter);    // Commence the read operation.

		if(rd_status < 0)
		{
			if(errno == EWOULDBLOCK || errno == EAGAIN || \
					errno == EINTR)   // Probable recoverable errors, act as caller requested.
			{
				if(mode == MID_MODE_SOCK_READ_AUTO_RETRY)  // If mode is auto-retry then read again.
					continue;
				else
				{
					status != NULL ? *status = rd_counter : 0;
					return MID_ERROR_SOCK_READ_RETRY;
				}
			}
			else if(errno == EFAULT)  // Faulty buffer
			{
				status != NULL ? *status = rd_counter : 0;
				return MID_ERROR_SOCK_READ_BUFFER_FULL;
			}
			else  // Fatal Error, report back.
			{
				status != NULL ? *status = rd_counter : 0;
				return MID_ERROR_SOCK_READ_ERROR;
			}
		}
		else if(rd_status > 0)   // If some bytes were read.
		{
			rd_counter = rd_counter + rd_status;

			if(rd_counter == m_data->len)   // No more space left, report back
			{
				status != NULL ? *status = rd_counter : 0;
				return MID_ERROR_SOCK_READ_BUFFER_FULL;
			}

			if(mode == MID_MODE_SOCK_READ_AUTO_RETRY)  // Act as caller requested.
				continue;
			else
			{
				status != NULL ? *status = rd_counter : 0;
				return MID_ERROR_SOCK_READ_RETRY;
			}
		}

		// Else EOF reached, break.

		break;
	}

	status != NULL ? *status = rd_counter : 0;
	return MID_ERROR_SOCK_READ_NONE;
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
