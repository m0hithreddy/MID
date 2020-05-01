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

int ssl_sock_write(SSL* ssl,struct mid_data* n_data)
{
#ifdef LIBSSL_SANE
	if(ssl==NULL || n_data->data==NULL || n_data->len==0)
		return 0;

	return SSL_write(ssl,n_data->data,n_data->len);
#else
	SSL_quit();
#endif
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
