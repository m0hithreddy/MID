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
	if(mid_cli == NULL)
		return 0;

	const SSL_METHOD* method = TLS_client_method();

	SSL_CTX* ctx = SSL_CTX_new(method);

	if(ctx == NULL)
		goto init_error;

	mid_cli->ssl = SSL_new(ctx);

	SSL_set_fd(mid_cli->ssl, mid_cli->sockfd);

	if(mid_cli->hostname != NULL)
		SSL_set_tlsext_host_name(mid_cli->ssl, mid_cli->hostname); // Hostname used during certificate verification

	if(SSL_connect(mid_cli->ssl) < 0)
		goto init_error;

	return 1;

	init_error:

	mid_cli->ssl = NULL;
	return 0;
#else
	SSL_quit();
#endif

}

int close_mid_ssl(struct mid_client* mid_cli)
{
#ifdef LIBSSL_SANE
	if(mid_cli == NULL)
		return 0;

	int ssl_status = SSL_shutdown(mid_cli->ssl);   // May not actually shutdown for non-blocking sockets.

	if(ssl_status == 0 || ssl_status == 1)  // Return success when uni-directional or bi-directional shutdown happens.
		return 1;

	return 0;
#else
	SSL_quit();
#endif
}

void free_mid_ssl(struct mid_client* mid_cli)
{
#ifdef LIBSSL_SANE
	if(mid_cli == NULL)
		return;

	SSL_free(mid_cli->ssl);
#else
	SSL_quit();
#endif
}

int mid_ssl_socket_write(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status)
{
	if(mid_cli == NULL || mid_cli->sockfd < 0 || mid_cli->ssl == NULL || \
			m_data == NULL || m_data->data == NULL || m_data->len <= 0 ) {  // Invalid input.

		status != NULL ? *status = 0 : 0;
		return MID_ERROR_SOCK_WRITE_INVAL;
	}

	long wr_status = 0, wr_counter = 0;

	fd_set wr_set, tp_set;
	FD_ZERO(&wr_set);
	FD_SET(mid_cli->sockfd, &wr_set);

	int maxfds = mid_cli->sockfd + 1, sl_status = 0, \
			ssl_err = 0, rel_err = 0;

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

		wr_status = SSL_write(mid_cli->ssl, m_data->data + wr_counter, \
				m_data->len - wr_counter);  // Commence the write operation.

		if(wr_status <= 0)  // Error reported by SSL_write.
		{
			ssl_err = SSL_get_error(mid_cli->ssl, wr_status);

			if(ssl_err == SSL_ERROR_WANT_WRITE || \
					ssl_err == SSL_ERROR_WANT_READ) {  // Probable recoverable errors.

				if(mode == MID_MODE_SOCK_WRITE_AUTO_RETRY)
					continue;
				else
				{
					status != NULL ? *status = wr_counter : 0;
					return MID_ERROR_SOCK_WRITE_RETRY;
				}
			}
			else if(ssl_err == SSL_ERROR_ZERO_RETURN)  // TLS session closed cleanly
			{
				status != NULL ? *status = wr_counter : 0;
				return MID_ERROR_SOCK_WRITE_NONE;
			}
			else if(ssl_err == SSL_ERROR_SYSCALL)   // Check for real error, and report status.
			{
				status != NULL ? *status = wr_counter : 0;

				rel_err = ERR_get_error();
				if(rel_err == 0)  // No real error.
					return MID_ERROR_SOCK_WRITE_NONE;
				else
					return MID_ERROR_SOCK_WRITE_ERROR;
			}
			else   // Fatal Error.
			{
				status != NULL ? *status = wr_counter : 0;
				return MID_ERROR_SOCK_WRITE_ERROR;
			}
		}

		wr_counter = wr_counter + wr_status;

		if(wr_counter < m_data->len)  // If fewer bytes are transfered.
		{
			if(mode == MID_MODE_SOCK_WRITE_AUTO_RETRY)  // Act as caller requested.
				continue;
			else
			{
				status != NULL ? *status = wr_counter : 0;
				return MID_ERROR_SOCK_WRITE_RETRY;
			}
		}

		// Else all bytes are transfered, break.

		break;
	}

	status != NULL ? *status = wr_counter : 0;
	return MID_ERROR_SOCK_WRITE_NONE;
}

int mid_ssl_socket_read(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status)
{
	if(mid_cli == NULL || mid_cli->sockfd <= 0 || mid_cli->ssl == NULL || \
			m_data == NULL || m_data->data == NULL || m_data->len <= 0) {  // Invalid input given.

		status != NULL ? *status = 0 : 0;
		return MID_ERROR_SOCK_READ_INVAL;
	}

	long rd_status = 0, rd_counter = 0;

	fd_set rd_set, tp_set;
	FD_ZERO(&rd_set);
	FD_SET(mid_cli->sockfd, &rd_set);

	int maxfds = mid_cli->sockfd + 1, sl_status = 0, \
			ssl_err = 0, rel_err = 0;

	for( ;  ; )
	{
		if(mode == MID_MODE_SOCK_READ_AUTO_RETRY) {  /* If mode is auto-retry and non blocking sockets are used,
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

		rd_status = SSL_read(mid_cli->ssl, m_data->data + rd_counter, \
				m_data->len - rd_counter);  // Commence the read operation.

		if(rd_status <= 0)
		{
			ssl_err = SSL_get_error(mid_cli->ssl, rd_status);

			if(ssl_err == SSL_ERROR_WANT_READ || \
					ssl_err == SSL_ERROR_WANT_WRITE) {   // A probable recoverable errors.

				if(mode == MID_MODE_SOCK_READ_AUTO_RETRY)
					continue;
				else
				{
					status != NULL ? *status = rd_counter : 0;
					return MID_ERROR_SOCK_READ_RETRY;
				}
			}
			else if(ssl_err == SSL_ERROR_ZERO_RETURN)  // TLS session closed cleanly.
			{
				status != NULL ? *status = rd_counter : 0;
				return MID_ERROR_SOCK_READ_NONE;
			}
			else if(ssl_err == SSL_ERROR_SYSCALL)  // Check if error occurred, and report back.
			{
				status != NULL ? *status = rd_counter : 0;

				rel_err = ERR_get_error();
				if(rel_err == 0)
					return MID_ERROR_SOCK_READ_NONE;
				else
					return MID_ERROR_SOCK_READ_ERROR;
			}
			else  // Fatal error, report back.
			{
				status != NULL ? *status = rd_counter : 0;
				return MID_ERROR_SOCK_READ_ERROR;
			}
		}

		rd_counter = rd_counter + rd_status;

		if(rd_counter == m_data->len)  // If buffer is full.
		{
			status != NULL ? *status = rd_counter : 0;
			return MID_ERROR_SOCK_READ_BUFFER_FULL;
		}

		if(mode == MID_MODE_SOCK_READ_AUTO_RETRY)  // After reading, act as user requested.
			continue;
		else
		{
			status != NULL ? *status = rd_counter : 0;
			return MID_ERROR_SOCK_READ_RETRY;
		}
	}

	status != NULL ? *status = rd_counter : 0;
	return MID_ERROR_SOCK_READ_NONE;
}

struct mid_data* ssl_sock_read(SSL* ssl)
{
#ifdef LIBSSL_SANE
	if(ssl==NULL)
		return NULL;

	struct mid_bag *bag=create_mid_bag();
	struct mid_data *n_data=(struct mid_data*)malloc(sizeof(struct mid_data));

	n_data->data=(char*)malloc(sizeof(char)*MAX_TRANSACTION_SIZE);

	while(1)
	{
		int status=SSL_read(ssl,n_data->data,MAX_TRANSACTION_SIZE);

		if(status>0)
		{
			n_data->len=status;
			place_mid_data(bag, n_data);
		}
		else
			break;
	}

	n_data=flatten_mid_bag(bag);

	return n_data;
#else
	SSL_quit();
#endif
}
