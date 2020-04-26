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

#ifdef LIBSSL_SANE
SSL* ssl_open_connection(int sockfd,char* hostname)
{
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	const SSL_METHOD* method=TLS_method();

	SSL_CTX* ctx=SSL_CTX_new(method);

	if(ctx==NULL)
		return NULL;

	SSL* ssl=SSL_new(ctx);

	SSL_set_fd(ssl,sockfd);

	if(hostname!=NULL)
		SSL_set_tlsext_host_name(ssl, hostname); // Host used during certificate verification

	if(SSL_connect(ssl)<0)
	{
		return NULL;
	}

	return ssl;
}

int ssl_sock_write(SSL* ssl,struct network_data* n_data)
{
	if(ssl==NULL || n_data->data==NULL || n_data->len==0)
		return 0;

	return SSL_write(ssl,n_data->data,n_data->len);
}

struct network_data* ssl_sock_read(SSL* ssl)
{
	if(ssl==NULL)
		return NULL;

	struct mid_bag *bag=create_mid_bag();
	struct network_data *n_data=(struct network_data*)malloc(sizeof(struct network_data));

	n_data->data=(char*)malloc(sizeof(char)*MAX_TRANSACTION_SIZE);

	while(1)
	{
		int status=SSL_read(ssl,n_data->data,MAX_TRANSACTION_SIZE);

		if(status>0)
		{
			n_data->len=status;
			place_data(bag, n_data);
		}
		else
			break;
	}

	n_data=flatten_mid_bag(bag);

	return n_data;
}
#else
SSL* ssl_open_connection(int sockfd,char* hostname)
{
	https_quit();
	return NULL;
}

int ssl_sock_write(SSL* ssl,struct network_data* n_data)
{
	https_quit();
	return -1;
}

struct network_data* ssl_sock_read(SSL* ssl)
{
	https_quit();
	return NULL;
}

int SSL_read(void* ran1,void* ran2,long ran3)
{
	https_quit();
	return -1;
}

#endif
