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
#include<sys/signalfd.h>

struct mid_client* sig_create_mid_client(struct mid_interface* mid_if, struct parsed_url* purl, sigset_t* sigmask)
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
	mid_cli->io_timeout = args->io_timeout;
	mid_cli->sigmask = sigmask;

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

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
#ifdef LIBSSL_SANE
		int ssl_status = 0;
#endif

		mid_cli->sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

		if(mid_cli->sockfd == -1)
			goto next;

		/* Set the socket options */

		if(setsockopt(mid_cli->sockfd, IPPROTO_TCP, TCP_SYNCNT, \
				&args->max_tcp_syn_retransmits, sizeof(int)) != 0)   // TCP SYN retransmit count
			goto next;

		if(mid_cli->if_name != NULL && mid_cli->if_addr!=NULL)
		{
			if(setsockopt(mid_cli->sockfd, SOL_SOCKET, SO_BINDTODEVICE, \
					mid_cli->if_name, strlen(mid_cli->if_name)) != 0)   // Set the interface
			goto next;

			/* Bind to the interface */

			struct sockaddr_storage cli_addr;
			memset(&cli_addr, 0, sizeof(struct sockaddr_storage));  // Initialize the client sockaddr structure.

			cli_addr.ss_family = rp->ai_family;   // Set family.

			if(rp->ai_family == AF_INET)   // Set port.
				((struct sockaddr_in*)&cli_addr)->sin_port = htons(0);
			else if(rp->ai_family == AF_INET6)
				((struct sockaddr_in6*)&cli_addr)->sin6_port = htons(0);
			else
				goto next;

			inet_pton(rp->ai_family, mid_cli->if_addr, rp->ai_family == AF_INET ? \
					(void*)(&((struct sockaddr_in*)&cli_addr)->sin_addr) : \
					(void*)(&((struct sockaddr_in6*)&cli_addr)->sin6_addr));  // Set IPv{4, 6} address.

			if(bind(mid_cli->sockfd, (struct sockaddr*)&cli_addr, sizeof(struct sockaddr_storage)) != 0)   // call the bind
				goto next;
		}

		/* Set the socket in non-blocking mode (for implementing connect timeout) */

		int sock_args = fcntl(mid_cli->sockfd, F_GETFL);

		if(sock_args < 0)
			goto next;

		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args | O_NONBLOCK) < 0)
			goto next;

		/* Start the connect procedure */

		if(connect(mid_cli->sockfd, rp->ai_addr, rp->ai_addrlen) == 0 )  // If connected.
			goto success;

		if(errno == EINPROGRESS || errno == EINTR)  // If connection in progress or interrupted.
		{

			/* Timeout initializations */

			struct timeval *timeo, tp_timeo;

			if(mid_cli->io_timeout > 0)
			{
				timeo = (struct timeval*)malloc(sizeof(struct timeval));
				timeo->tv_sec = mid_cli->io_timeout;
				timeo->tv_usec = 0;
			}
			else
				timeo = NULL;

			/* Select descriptor or timeout */

			int sl_status = 0;

			for( ; ; )
			{
				fd_set rd_set, wr_set;
				FD_ZERO(&rd_set);
				FD_SET(mid_cli->sockfd, &rd_set);
				wr_set = rd_set;

				sl_status = select(mid_cli->sockfd + 1, &rd_set, &wr_set, NULL, \
						timeo == NULL ? NULL : (tp_timeo = *timeo , &tp_timeo));

				if(sl_status == 0)  // Timeout
					goto next;
				else if(sl_status < 0)  // Error reported by select.
				{
					if(errno == EINTR)  // interrupted, retry select (timeout resets).
						continue;
					else  // Fatal error, in select.
						goto next;
				}

				if(FD_ISSET(mid_cli->sockfd, &rd_set) || FD_ISSET(mid_cli->sockfd, &wr_set))  // connect success or fail.
				{
					int error, len = sizeof(int);

					if(getsockopt(mid_cli->sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)  // Solaris way of reporting error.
						goto next;

					if(error != 0)  // Failed to connect.
						goto next;

					goto success;  // Connect success.
				}
				else  // just in case
					goto next;
			}
		}
		else   // Fatal error, in connect.
			goto next;


		success:

		/* If SSL required then setup SSL session */

#ifdef LIBSSL_SANE
		if(mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS)
		{
			if((ssl_status = init_mid_ssl(mid_cli)) == 0)
				goto next;
		}
#endif

		/* Revert back the socket mode */

		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args) < 0)
			goto next;

		/*  Set the Host-IP (only if AF_INET or AF_INET6) */

		if(rp->ai_family == AF_INET || rp->ai_family == AF_INET6)
		{
			mid_cli->hostip = (char*)malloc(sizeof(char)*(rp->ai_family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN));
			inet_ntop(rp->ai_family, rp->ai_addr, mid_cli->hostip, rp->ai_family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN);
		}

		break;

		next:    /* Try the next addrinfo structure*/

		close_mid_client(mid_cli);
	}

	freeaddrinfo(result);

	if (rp == NULL)      // No address succeeded, return.
		goto init_error;

	return 1;

	init_error:

	close_mid_client(mid_cli);

	mid_cli->hostip = NULL;

	return 0;
}

int close_mid_client(struct mid_client* mid_cli)
{
	if(mid_cli == NULL)
		return 0;

	int return_status = 1;

	/* Close SSL connection */

#ifdef LIBSSL_SANE
	if(mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS)
	{
		if(close_mid_ssl(mid_cli) != 1)
			return_status = 0;
	}
#endif

	/* Close socket connection */

	if(mid_cli->sockfd >= 0)
	{
		if(close(mid_cli->sockfd) != 0)
			return_status = 0;

		mid_cli->sockfd = -1;
	}
	else
		return_status = 0;

	return return_status;
}

int free_mid_client(struct mid_client** mid_cli)
{
	if(mid_cli == NULL || *mid_cli == NULL)
		return 0;

	/* Close the connections and Free SSL structure (If set) */

	int return_status = close_mid_client(*mid_cli);

	/* Free the pointers */

	free((*mid_cli)->if_name);
	free((*mid_cli)->if_addr);
	free((*mid_cli)->hostname);
	free((*mid_cli)->port);
	free((*mid_cli)->hostip);

	/* Free the struct */

	free(*mid_cli);
	*mid_cli = NULL;

	return return_status;
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
			m_data->data == NULL || m_data->len <=0 ) {  // Invalid input.

		status != NULL ? *status = 0 : 0;
		return MID_ERROR_SOCK_WRITE_INVAL;
	}

	/* Set the socket mode */

	int sock_args = fcntl(mid_cli->sockfd, F_GETFL);

	if(sock_args < 0)
	{
		status != NULL ? *status = 0 : 0;
		return MID_ERROR_SOCK_WRITE_INVAL;
	}

	int no_block = sock_args & O_NONBLOCK;

	if(!no_block) {  /* If socket is in blocking mode, in the worst situations the write call may get
	blocked. So put the socket in non-blocking mode. */

		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args | O_NONBLOCK) < 0)
		{
			status != NULL ? *status = 0 : 0;
			return MID_ERROR_SOCK_WRITE_INVAL;
		}
	}

	/* Timeout initializations */

	struct timeval *wr_time, tp_time;

	if(mid_cli->io_timeout > 0)
	{
		wr_time = (struct timeval*)malloc(sizeof(struct timeval));
		wr_time->tv_sec = mid_cli->io_timeout;
		wr_time->tv_usec  = 0;
	}
	else
		wr_time = NULL;

	/* Select initializations */

	fd_set wr_set, tw_set;
	FD_ZERO(&wr_set);
	FD_SET(mid_cli->sockfd, &wr_set);

	int maxfds = mid_cli->sockfd + 1, sl_status = 0;

	/* Signal mask initializations */

	int sigfd = -1;
	struct signalfd_siginfo sigbuf;
	fd_set rd_set, tr_set;
	FD_ZERO(&rd_set);

	if(mid_cli->sigmask != NULL)
	{
		sigfd = signalfd(-1, mid_cli->sigmask, 0);

		if(sigfd < 0)
		{
			status != NULL ? *status = 0 : 0;
			return MID_ERROR_SOCK_WRITE_INVAL;
		}

		FD_SET(sigfd, &rd_set);

		maxfds = sigfd > mid_cli->sockfd ? sigfd + 1 : mid_cli->sockfd + 1;
	}

	/* Fetch the data */

	long wr_status = 0, wr_counter = 0;

	int return_status = MID_ERROR_SOCK_WRITE_NONE;

	for( ;  ; )
	{

		tw_set = wr_set;

		sl_status = select(maxfds, mid_cli->sigmask == NULL ? NULL : (tr_set = rd_set, &tr_set), &tw_set, NULL,\
				wr_time == NULL ? NULL : (tp_time = *wr_time, &tp_time));  // Select the descriptor or timeout.

		if(sl_status < 0)  // Error reported by select.
		{
			if(errno == EINTR)  // If interrupted by signal, act as caller requested.
			{
				if(mode == MID_MODE_SOCK_WRITE_AUTO_RETRY)
					continue;
				else
				{
					return_status = MID_ERROR_SOCK_WRITE_RETRY;
					goto write_return;
				}
			}
			else  // Fatal error in select, report back.
			{
				return_status = MID_ERROR_SOCK_WRITE_ERROR;
				goto write_return;
			}
		}
		else if(sl_status == 0)  // Timeout.
		{
			return_status = MID_ERROR_SOCK_WRITE_TIMEOUT;
			goto write_return;
		}

		if(mid_cli->sigmask != NULL)
		{
			if(FD_ISSET(sigfd, &tr_set))  // Signal received.
			{
				read(sigfd, &sigbuf, sizeof(struct signalfd_siginfo));

				return_status = MID_ERROR_SOCK_WRITE_SIGRCVD;
				goto write_return;
			}
		}

		if(!FD_ISSET(mid_cli->sockfd, &tw_set))  // If socket is not set.
		{
			return_status = MID_ERROR_SOCK_WRITE_ERROR;
			goto write_return;
		}

		wr_status = write(mid_cli->sockfd, m_data->data + wr_counter, \
				m_data->len - wr_counter);    // Commence the write operation.

		if(wr_status < 0)  // If the error condition.
		{
			if(errno == EINTR) //Signal interrupt. Both [non-]block sockets, act as user requested.
			{
				if(mode == MID_MODE_SOCK_WRITE_AUTO_RETRY)
					continue;
				else
				{
					return_status = MID_ERROR_SOCK_WRITE_RETRY;
					goto write_return;
				}
			}
			else if(errno == EWOULDBLOCK || errno == EAGAIN) // Retry again. non-block sockets only
			{
				if(mode == MID_MODE_SOCK_WRITE_AUTO_RETRY)
					continue;
				else if(mode == MID_MODE_SOCK_WRITE_PARTIAL_WRITE && !no_block)  // We made it non-blocking, retry.
					continue;
				else
				{
					return_status = MID_ERROR_SOCK_WRITE_RETRY;
					goto write_return;
				}
			}
			else   // Fatal Error, report back. Both [non-]block sockets.
			{
				return_status = MID_ERROR_SOCK_WRITE_ERROR;
				goto write_return;
			}
		}

		if(wr_status > 0)
			wr_counter = wr_counter + wr_status;

		if(wr_counter < m_data->len)   // If fewer bytes transfered. Both [non-]block sockets.
		{
			if(mode == MID_MODE_SOCK_WRITE_AUTO_RETRY)   // Act as user requested.
				continue;
			else
			{
				return_status = MID_ERROR_SOCK_WRITE_RETRY;
				goto write_return;
			}
		}

		// Else all bytes are transferred, break;

		return_status = MID_ERROR_SOCK_WRITE_NONE;
		goto write_return;
	}

	/* Return procedures */

	write_return:

	if(!no_block)  // Revert back the socket mode.
	{
		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args) < 0)
			return_status = MID_ERROR_SOCK_WRITE_ERROR;
	}

	if(mid_cli->sigmask != NULL && sigfd >= 0)  // If signal_fd opened then close.
		close(sigfd);

	status != NULL ? *status = wr_counter : 0;

	return return_status;
}

int mid_socket_read(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status)
{
	if(mid_cli == NULL || mid_cli->sockfd < 0 || m_data == NULL || \
			m_data->data == NULL || m_data->len <= 0) {  // Invalid input.

		status != NULL ? *status = 0 : 0;
		return MID_ERROR_SOCK_READ_INVAL;
	}

	/* Set the socket mode */

	int sock_args = fcntl(mid_cli->sockfd, F_GETFL);

	if(sock_args < 0)
	{
		status != NULL ? *status = 0 : 0;
		return MID_ERROR_SOCK_READ_INVAL;
	}

	int no_block = sock_args & O_NONBLOCK;

	if(!no_block) {  /* If socket is in blocking mode, in the worst situations the read call may get
	blocked (Packets being dropped after select returns). So put the socket in non-blocking mode. */

		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args | O_NONBLOCK) < 0)
		{
			status != NULL ? *status = 0 : 0;
			return MID_ERROR_SOCK_READ_INVAL;
		}
	}

	/* Timeout initializations */

	struct timeval *rd_time, tp_time;

	if(mid_cli->io_timeout > 0)
	{
		rd_time = (struct timeval*)malloc(sizeof(struct timeval));
		rd_time->tv_sec = mid_cli->io_timeout;
		rd_time->tv_usec  = 0;
	}
	else
		rd_time = NULL;

	/* Select initializations */

	fd_set rd_set, tr_set;
	FD_ZERO(&rd_set);
	FD_SET(mid_cli->sockfd, &rd_set);

	int maxfds = mid_cli->sockfd + 1, sl_status = 0;

	/* Signal mask initializations */

	int sigfd = -1;
	struct signalfd_siginfo sigbuf;

	if(mid_cli->sigmask != NULL)
	{
		sigfd = signalfd(-1, mid_cli->sigmask, 0);

		if(sigfd < 0)
		{
			status != NULL ? *status = 0 : 0;
			return MID_ERROR_SOCK_READ_INVAL;
		}

		FD_SET(sigfd, &rd_set);

		maxfds = sigfd > mid_cli->sockfd ? sigfd + 1 : mid_cli->sockfd + 1;
	}

	/* Fetch the data */

	long rd_status = 0, rd_counter = 0;

	int return_status = MID_ERROR_SOCK_READ_NONE;

	for( ;  ; )
	{
		tr_set = rd_set;

		sl_status = select(maxfds, &tr_set, NULL, NULL,\
				rd_time == NULL ? NULL : (tp_time = *rd_time, &tp_time));  // Select the descriptor or timeout.

		if(sl_status < 0)  // Error reported by select.
		{
			if(errno == EINTR)  // If interrupted by signal, act as caller requested.
			{
				if(mode == MID_MODE_SOCK_READ_AUTO_RETRY)
					continue;
				else
				{
					return_status = MID_ERROR_SOCK_READ_RETRY;
					goto read_return;
				}
			}
			else  // Fatal error in select, report back.
			{
				return_status = MID_ERROR_SOCK_READ_ERROR;
				goto read_return;
			}
		}
		else if(sl_status == 0)  // Timeout.
		{
			return_status = MID_ERROR_SOCK_READ_TIMEOUT;
			goto read_return;
		}

		if(mid_cli->sigmask != NULL)
		{
			if(FD_ISSET(sigfd, &tr_set))  // Signal received.
			{
				read(sigfd, &sigbuf, sizeof(struct signalfd_siginfo));

				return_status = MID_ERROR_SOCK_READ_SIGRCVD;
				goto read_return;
			}
		}

		if(!FD_ISSET(mid_cli->sockfd, &tr_set))  // If socket is not set.
		{
			return_status = MID_ERROR_SOCK_READ_ERROR;
			goto read_return;
		}

		rd_status = read(mid_cli->sockfd, m_data->data + rd_counter, \
				m_data->len - rd_counter);    // Commence the read operation.

		if(rd_status < 0)
		{
			if(errno == EINTR) //Signal interrupt. Both [non-]block sockets, act as user requested.
			{
				if(mode == MID_MODE_SOCK_READ_AUTO_RETRY)
					continue;
				else
				{
					return_status = MID_ERROR_SOCK_READ_RETRY;
					goto read_return;
				}
			}
			else if(errno == EWOULDBLOCK || errno == EAGAIN) // Retry again. non-block sockets only
			{
				if(mode == MID_MODE_SOCK_READ_AUTO_RETRY)
					continue;
				else if(mode == MID_MODE_SOCK_READ_PARTIAL_READ && !no_block)  // We made it non-blocking, retry.
					continue;
				else
				{
					return_status = MID_ERROR_SOCK_READ_RETRY;
					goto read_return;
				}
			}
			else if(errno == EFAULT)  // Faulty buffer. Both [non-]block sockets.
			{
				return_status = MID_ERROR_SOCK_READ_BUFFER_FULL;
				goto read_return;
			}
			else  // Fatal Error, report back. Both [non-]block sockets.
			{
				return_status = MID_ERROR_SOCK_READ_ERROR;
				goto read_return;
			}
		}
		else if(rd_status > 0)   // If some bytes were read. Both [non-]block sockets.
		{
			rd_counter = rd_counter + rd_status;

			if(rd_counter == m_data->len)   // No more space left, report back
			{
				return_status = MID_ERROR_SOCK_READ_BUFFER_FULL;
				goto read_return;
			}

			if(mode == MID_MODE_SOCK_READ_AUTO_RETRY)  // Act as caller requested.
				continue;
			else
			{
				return_status = MID_ERROR_SOCK_READ_RETRY;
				goto read_return;
			}
		}

		// Else EOF reached, break.

		return_status = MID_ERROR_SOCK_READ_NONE;
		goto read_return;
	}

	/* Return procedures */

	read_return:

	if(!no_block)  // Revert back the socket mode.
	{
		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args) < 0)
			return_status = MID_ERROR_SOCK_READ_ERROR;
	}

	if(mid_cli->sigmask != NULL && sigfd >= 0)  // If signal_fd opened then close.
		close(sigfd);

	status != NULL ? *status = rd_counter : 0;  // Update status.

	return return_status;
}
