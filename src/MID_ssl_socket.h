/*
 * MID_ssl_socket.h
 *
 *  Created on: 10-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_SSL_SOCKET_H_
#define MID_SSL_SOCKET_H_

#include"MID_socket.h"

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

#ifdef LIBSSL_SANE
#include<openssl/ssl.h>
#include<openssl/err.h>
#include<openssl/evp.h>
#include<openssl/md5.h>
#else
typedef void* SSL;
int SSL_read(void* ran1,void* ran2,long ran3);
#endif

SSL* ssl_open_connection(int sockfd,char* hostname);

int ssl_sock_write(SSL* ssl,struct mid_data* n_data);

struct mid_data* ssl_sock_read(SSL* ssl);

#endif /* MID_SSL_SOCKET_H_ */
