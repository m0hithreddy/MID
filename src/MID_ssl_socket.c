/*
 * MID_ssl_socket.c
 *
 *  Created on: 10-Mar-2020
 *      Author: Mohith Reddy
 */

#include"MID_socket.h"
#include"MID_ssl_socket.h"
#include"MID_structures.h"
#include"MID_http.h"
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/signalfd.h>
#include<unistd.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

void setup_ssl()
{
#ifdef LIBSSL_SANE
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
#else
	SSL_quit();
#endif
}

int init_mid_ssl(struct mid_client* mid_cli)
{
#ifdef LIBSSL_SANE

	if(mid_cli == NULL || mid_cli->sockfd < 0)
		return MID_ERROR_INVAL;

	int sock_args = -1, ssl_status = 0, \
			return_status = MID_ERROR_NONE, sigfd = -1;   // Variables used by init_return. (so declare at beginning).

	const SSL_METHOD* method = TLS_client_method();

	SSL_CTX* ctx = SSL_CTX_new(method);  // SSL Context.

	if(ctx == NULL)
	{
		return_status = MID_ERROR_FATAL;
		goto init_return;
	}

	mid_cli->ssl = SSL_new(ctx);  // New SSL object.

	SSL_set_fd(mid_cli->ssl, mid_cli->sockfd);  // Link socket fd and SSL.

	if(mid_cli->hostname != NULL)
		SSL_set_tlsext_host_name(mid_cli->ssl, mid_cli->hostname); // Host-Name used during certificate verification

	/* Set the socket mode to non-blocking to implement SSL_connect timeout */

	sock_args = fcntl(mid_cli->sockfd, F_GETFL);  // get flags

	if(sock_args < 0)
	{
		return_status = MID_ERROR_FATAL;
		goto init_return;
	}

	if(fcntl(mid_cli->sockfd, F_SETFL, sock_args | O_NONBLOCK) < 0)  // set flags
	{
		return_status = MID_ERROR_FATAL;
		goto init_return;
	}

	/* Start SSL_connect procedure */

	ssl_status = SSL_connect(mid_cli->ssl);

	if(ssl_status == 1)  // If success
	{
		return_status = MID_ERROR_NONE;
		goto init_return;
	}

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

	/* Select variables initializations */

	fd_set wr_set, tw_set;
	FD_ZERO(&wr_set);
	FD_SET(mid_cli->sockfd, &wr_set);

	int maxfds = mid_cli->sockfd + 1;

	/* Sigmask initializations */

	sigfd = -1;
	struct signalfd_siginfo sigbuf;

	fd_set rd_set, tr_set;
	FD_ZERO(&rd_set);

	if(mid_cli->sigmask != NULL)
	{
		sigfd = signalfd(-1, mid_cli->sigmask, 0);

		if(sigfd < 0)
		{
			return_status = MID_ERROR_FATAL;
			goto init_return;
		}

		FD_SET(sigfd, &rd_set);
		maxfds = sigfd > mid_cli->sockfd ? sigfd + 1 : mid_cli->sockfd + 1;
	}

	/* Loop for non-blocking SSL_connect */

	int ssl_err = 0, sl_status = 0;

	for( ; ; )
	{
		if(ssl_status == 1)  // If SSL_connect succeeded.
		{
			return_status = MID_ERROR_NONE;
			goto init_return;
		}
		else if(ssl_status == 0)  // If SSL shutdown
		{
			return_status = MID_ERROR_FATAL;
			goto init_return;
		}

		ssl_err = SSL_get_error(mid_cli->ssl, ssl_status);   // get SSL errors.

		if(ssl_err != SSL_ERROR_WANT_READ && ssl_err != SSL_ERROR_WANT_WRITE) // If SSL fatal errors.
		{
			return_status = MID_ERROR_FATAL;
			goto init_return;
		}

		select_procedure:

		tr_set = rd_set;

		if(ssl_err == SSL_ERROR_WANT_READ)
			FD_SET(mid_cli->sockfd, &tr_set);

		sl_status = select(maxfds, (mid_cli->sigmask != NULL || ssl_err == SSL_ERROR_WANT_READ) ? &tr_set : NULL, \
				ssl_err == SSL_ERROR_WANT_WRITE ? (tw_set = wr_set, &tw_set) : NULL, NULL, \
				timeo == NULL ? NULL : (tp_timeo = *timeo, &tp_timeo));   // Select descriptor or timeout.

		if(sl_status == 0)  // Timeout.
		{
			return_status = MID_ERROR_TIMEOUT;
			goto init_return;
		}
		else if(sl_status < 0)  // Error in select.
		{
			if(errno == EINTR)  // retry select (timeout reset).
				goto select_procedure;
			else
			{
				return_status = MID_ERROR_FATAL;
				goto init_return;
			}
		}

		if(mid_cli->sigmask != NULL)
		{
			if(FD_ISSET(sigfd, &tr_set))  // If signals pointed by mid_cli->sigmask rcvd.
			{
				read(sigfd, &sigbuf, sizeof(struct signalfd_siginfo));

				return_status = MID_ERROR_SIGRCVD;
				goto init_return;
			}
		}

		if(ssl_err == SSL_ERROR_WANT_READ ? FD_ISSET(mid_cli->sockfd, &tr_set) : \
				FD_ISSET(mid_cli->sockfd, &tw_set)) // If socket descriptor ready.
		{
			ssl_status = SSL_connect(mid_cli->ssl);
			continue;
		}
		else  // No descriptor is set (just in case).
		{
			return_status = MID_ERROR_FATAL;
			goto init_return;
		}
	}

	init_return:

	if(sigfd >= 0)  // If sigfd is set close sigfd.
	{
		close(sigfd);
		sigfd = -1;
	}

	if(sock_args >= 0)  // Revert back the socket mode.
	{
		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args) < 0)
			return_status = MID_ERROR_FATAL;
	}

	if(return_status != MID_ERROR_NONE)
		close_mid_ssl(mid_cli);

	return return_status;
#else
	SSL_quit();
#endif
}

int close_mid_ssl(struct mid_client* mid_cli)
{
#ifdef LIBSSL_SANE
	if(mid_cli == NULL || mid_cli->ssl == NULL)
		return 0;

	int ssl_status = SSL_shutdown(mid_cli->ssl);   // May not actually shutdown for non-blocking sockets.

	SSL_free(mid_cli->ssl);  // Free the SSL structure.

	mid_cli->ssl = NULL;

	if(ssl_status == 0 || ssl_status == 1)  // Return success when uni-directional or bi-directional shutdown happens.
		return 1;

	return 0;
#else
	SSL_quit();
#endif
}

int mid_ssl_socket_write(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status)
{
#ifdef LIBSSL_SANE
	if(mid_cli == NULL || mid_cli->sockfd < 0 || mid_cli->ssl == NULL || \
			m_data == NULL || m_data->data == NULL || m_data->len <= 0 ) {  // Invalid input.

		status != NULL ? *status = 0 : 0;
		return MID_ERROR_INVAL;
	}


	/* Set the socket mode */

	int sock_args = fcntl(mid_cli->sockfd, F_GETFL);

	if(sock_args < 0)
		return MID_ERROR_FATAL;

	int no_block = sock_args & O_NONBLOCK;

	if(!no_block) {  /* If socket is in blocking mode, SSL_write call may get blocked.
	So put the socket in non-blocking mode. */

		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args | O_NONBLOCK) < 0)
			return MID_ERROR_FATAL;
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
			return MID_ERROR_INVAL;
		}

		FD_SET(sigfd, &rd_set);

		maxfds = sigfd > mid_cli->sockfd ? sigfd + 1 : mid_cli->sockfd + 1;
	}

	/* Fetch the data */

	long wr_status = 0, wr_counter = 0;

	int return_status = MID_ERROR_NONE, ssl_err = SSL_ERROR_WANT_WRITE, \
			rel_err = 0;

	for( ;  ; )
	{
		tr_set = rd_set;

		if(ssl_err == SSL_ERROR_WANT_READ)
			FD_SET(mid_cli->sockfd, &tr_set);

		sl_status = select(maxfds, (mid_cli->sigmask != NULL || ssl_err == SSL_ERROR_WANT_READ) ? &tr_set : NULL, \
				ssl_err == SSL_ERROR_WANT_WRITE ? (tw_set = wr_set, &tw_set) : NULL, NULL,\
				wr_time == NULL ? NULL : (tp_time = *wr_time, &tp_time));  // Select the descriptor or timeout.

		if(sl_status < 0)  // Error reported by select.
		{
			if(errno == EINTR)  // If interrupted by signal, act as caller requested.
			{
				if(mode == MID_MODE_AUTO_RETRY)
					continue;
				else
				{
					return_status = MID_ERROR_RETRY;
					goto write_return;
				}
			}
			else  // Fatal error in select, report back.
			{
				return_status = MID_ERROR_FATAL;
				goto write_return;
			}
		}
		else if(sl_status == 0)  // Timeout.
		{
			return_status = MID_ERROR_TIMEOUT;
			goto write_return;
		}

		if(mid_cli->sigmask != NULL)
		{
			if(FD_ISSET(sigfd, &tr_set))  // Signal received.
			{
				read(sigfd, &sigbuf, sizeof(struct signalfd_siginfo));

				return_status = MID_ERROR_SIGRCVD;
				goto write_return;
			}
		}

		if(ssl_err == SSL_ERROR_WANT_READ ? !FD_ISSET(mid_cli->sockfd, &tr_set) : \
				!FD_ISSET(mid_cli->sockfd, &tw_set))  // If socket is not set.
		{
			return_status = MID_ERROR_FATAL;
			goto write_return;
		}

		wr_status = SSL_write(mid_cli->ssl, m_data->data + wr_counter, \
				m_data->len - wr_counter);  // Commence the write operation.

		if(wr_status <= 0)  // Error reported by SSL_write.
		{
			ssl_err = SSL_get_error(mid_cli->ssl, wr_status);

			if(ssl_err == SSL_ERROR_WANT_WRITE || \
					ssl_err == SSL_ERROR_WANT_READ) // Probable recoverable errors. Only non-block sockets.
			{

				if(mode == MID_MODE_AUTO_RETRY)
					continue;
				else if(mode == MID_MODE_PARTIAL && !no_block)  // Because we made it non-block, retry.
					continue;
				else
				{
					return_status = MID_ERROR_RETRY;
					goto write_return;
				}
			}
			else if(ssl_err == SSL_ERROR_ZERO_RETURN)  // TLS session closed cleanly. Both [non-]block sockets.
			{
				return_status = MID_ERROR_NONE;
				goto write_return;
			}
			else if(ssl_err == SSL_ERROR_SYSCALL)   // Check error, and report back. Both [non-]block sockets.
			{
				rel_err = ERR_get_error();

				if(rel_err == 0)  // No real error.
					return_status = MID_ERROR_NONE;
				else
					return_status = MID_ERROR_FATAL;

				goto write_return;
			}
			else  // Fatal error, report back.  Both [non-]block sockets.
			{
				return_status = MID_ERROR_FATAL;
				goto write_return;
			}
		}

		wr_counter = wr_counter + wr_status;

		if(wr_counter < m_data->len)  // If fewer bytes are transfered. Both [non-]block sockets.
		{
			if(mode == MID_MODE_AUTO_RETRY)  // Act as caller requested.
				goto auto_retry;
			else
			{
				return_status = MID_ERROR_RETRY;
				goto write_return;
			}
		}

		// Else all bytes are transfered, break.

		return_status = MID_ERROR_NONE;
		goto write_return;

		auto_retry:

		ssl_err = SSL_ERROR_WANT_WRITE;
	}

	/* Return procedures */

	write_return:

	if(!no_block)  // Revert back the socket mode.
	{
		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args) < 0)
			return_status = MID_ERROR_FATAL;
	}

	if(mid_cli->sigmask != NULL && sigfd >= 0)  // If signal_fd opened then close.
		close(sigfd);

	status != NULL ? *status = wr_counter : 0;

	return return_status;
#else
	SSL_quit();
#endif
}

int mid_ssl_socket_read(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status)
{
#ifdef LIBSSL_SANE
	if(mid_cli == NULL || mid_cli->sockfd <= 0 || mid_cli->ssl == NULL || \
			m_data == NULL || m_data->data == NULL || m_data->len <= 0) {  // Invalid input given.

		status != NULL ? *status = 0 : 0;
		return MID_ERROR_INVAL;
	}

	/* Set the socket mode */

	int sock_args = fcntl(mid_cli->sockfd, F_GETFL);

	if(sock_args < 0)
		return MID_ERROR_FATAL;

	int no_block = sock_args & O_NONBLOCK;

	if(!no_block) {  /* If socket is in blocking mode, SSL_read call may get blocked
	(select looks at encrypted buffer, not the decrypted one).So put the socket in non-blocking mode. */

		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args | O_NONBLOCK) < 0)
			return MID_ERROR_FATAL;
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
			return MID_ERROR_INVAL;
		}

		FD_SET(sigfd, &rd_set);

		maxfds = sigfd > mid_cli->sockfd ? sigfd + 1 : mid_cli->sockfd + 1;
	}

	/* Fetch the data */

	long rd_status = 0, rd_counter = 0;

	int return_status = MID_ERROR_NONE, ssl_err = SSL_ERROR_WANT_READ,\
			rel_err = 0;

	for( ;  ; )
	{
		tr_set = rd_set;

		if(ssl_err == SSL_ERROR_WANT_READ)
			FD_SET(mid_cli->sockfd, &tr_set);

		sl_status = select(maxfds, (ssl_err == SSL_ERROR_WANT_READ || mid_cli->sigmask != NULL) ? &tr_set : NULL, \
				ssl_err == SSL_ERROR_WANT_WRITE ? (tw_set = wr_set, &tw_set) : NULL, NULL,\
				rd_time == NULL ? NULL : (tp_time = *rd_time, &tp_time));  // Select the descriptor or timeout.

		if(sl_status < 0)  // Error reported by select.
		{
			if(errno == EINTR)  // If interrupted by signal, act as caller requested.
			{
				if(mode == MID_MODE_AUTO_RETRY)
					continue;
				else
				{
					return_status = MID_ERROR_RETRY;
					goto read_return;
				}
			}
			else  // Fatal error in select, report back.
			{
				return_status = MID_ERROR_FATAL;
				goto read_return;
			}
		}
		else if(sl_status == 0)  // Timeout.
		{
			return_status = MID_ERROR_TIMEOUT;
			goto read_return;
		}

		if(mid_cli->sigmask != NULL)
		{
			if(FD_ISSET(sigfd, &tr_set))  // Signal received.
			{
				read(sigfd, &sigbuf, sizeof(struct signalfd_siginfo));

				return_status = MID_ERROR_SIGRCVD;
				goto read_return;
			}
		}

		if(ssl_err == SSL_ERROR_WANT_READ ? !FD_ISSET(mid_cli->sockfd, &tr_set) : \
				!FD_ISSET(mid_cli->sockfd, &tw_set))  // If socket is not set.
		{
			return_status = MID_ERROR_FATAL;
			goto read_return;
		}

		rd_status = SSL_read(mid_cli->ssl, m_data->data + rd_counter, \
				m_data->len - rd_counter);  // Commence the read operation.

		if(rd_status <= 0)
		{
			ssl_err = SSL_get_error(mid_cli->ssl, rd_status);

			if(ssl_err == SSL_ERROR_WANT_READ || \
					ssl_err == SSL_ERROR_WANT_WRITE) // Probable recoverable errors. Only non-block sockets.
			{

				if(mode == MID_MODE_AUTO_RETRY)
					continue;
				else if(mode == MID_MODE_PARTIAL && !no_block)  // Because we made it non-block, retry.
					continue;
				else
				{
					return_status = MID_ERROR_RETRY;
					goto read_return;
				}
			}
			else if(ssl_err == SSL_ERROR_ZERO_RETURN)  // TLS session closed cleanly. Both [non-]block sockets.
			{
				return_status = MID_ERROR_NONE;
				goto read_return;
			}
			else if(ssl_err == SSL_ERROR_SYSCALL)  // Check error, and report back. Both [non-]block sockets.
			{
				rel_err = ERR_get_error();

				if(rel_err == 0)
					return_status = MID_ERROR_NONE;
				else
					return_status = MID_ERROR_FATAL;

				goto read_return;
			}
			else  // Fatal error, report back.  Both [non-]block sockets.
			{
				return_status = MID_ERROR_FATAL;
				goto read_return;
			}
		}

		rd_counter = rd_counter + rd_status;

		if(rd_counter == m_data->len)  // If buffer is full.
		{
			return_status = MID_ERROR_BUFFER_FULL;
			goto read_return;
		}

		if(mode == MID_MODE_AUTO_RETRY)  // After reading, act as user requested. Both [non-]block sockets.
			goto auto_retry;
		else
		{
			return_status = MID_ERROR_RETRY;
			goto read_return;
		}

		auto_retry:

		ssl_err = SSL_ERROR_WANT_READ;
	}

	/* Return procedures */

	read_return:

	if(!no_block)  // Revert back the socket mode.
	{
		if(fcntl(mid_cli->sockfd, F_SETFL, sock_args) < 0)
			return_status = MID_ERROR_FATAL;
	}

	if(mid_cli->sigmask != NULL && sigfd >= 0)  // If signal_fd opened then close.
		close(sigfd);

	status != NULL ? *status = rd_counter : 0;  // Update status.

	return return_status;
#else
	SSL_quit();
#endif
}
