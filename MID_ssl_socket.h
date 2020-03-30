/*
 * MID_ssl_socket.h
 *
 *  Created on: 10-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_SSL_SOCKET_H_
#define MID_SSL_SOCKET_H_

#include"MID_socket.h"
#include<openssl/ssl.h>
#include<openssl/err.h>

SSL* ssl_open_connection(int sockfd,char* hostname);

int ssl_sock_write(SSL* ssl,struct network_data* n_data);

struct network_data* ssl_sock_read(SSL* ssl);

#endif /* MID_SSL_SOCKET_H_ */
