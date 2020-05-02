/*
 * MID_ssl_socket.h
 *
 *  Created on: 10-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_SSL_SOCKET_H_
#define MID_SSL_SOCKET_H_

#include"MID_socket.h"
#include"MID_err.h"

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
#define SSL_quit() do {\
	mid_err("\nMID: HTTPS URL encountered! MID is not built with the SSL support. Please recompile MID with the SSL support. Exiting...\n\n");\
	exit(3);\
} while(0)

typedef void SSL;
#endif

void setup_ssl();

int init_mid_ssl(struct mid_client* mid_cli);

int close_mid_ssl(struct mid_client* mid_cli);

void free_mid_ssl(struct mid_client* mid_cli);

int mid_ssl_socket_write(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status);

int mid_ssl_socket_read(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status);

int ssl_sock_write(SSL* ssl,struct mid_data* n_data);

struct mid_data* ssl_sock_read(SSL* ssl);

#endif /* MID_SSL_SOCKET_H_ */
