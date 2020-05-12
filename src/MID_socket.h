/*
 * MID_socket.h
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_SOCKET_H_
#define MID_SOCKET_H_

#define MAX_TRANSACTION_SIZE 65536
#define MID_DEFAULT_TCP_SYN_RETRANSMITS 2

#define MID_DEFAULT_IO_TIMEOUT 60
#define MID_DEFAULT_IPV4_SCHEME 1
#define MID_DEFAULT_IPV6_SCHEME 1

#define MID_CONSTANT_APPLICATION_PROTOCOL_HTTP 0
#define MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS 1
#define MID_CONSTANT_APPLICATION_PROTOCOL_UNKNOWN 2

#define MID_ERROR_NONE 0
#define MID_ERROR_RETRY 1
#define MID_ERROR_FATAL 2
#define MID_ERROR_INVAL 3
#define MID_ERROR_TIMEOUT 4
#define MID_ERROR_SIGRCVD 5
#define MID_ERROR_BUFFER_FULL 6
#define MID_ERROR_DNS 7
#define MID_ERROR_PROTOCOL_UNKOWN 8

#define MID_MODE_AUTO_RETRY 0
#define MID_MODE_PARTIAL 1

#include"MID_structures.h"
#include"MID_interfaces.h"
#include"url_parser.h"
#include<netinet/in.h>
#include<sys/socket.h>
#include<signal.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

#ifdef LIBSSL_SANE
#include<openssl/ssl.h>
#endif

struct mid_client
{
	char* if_name;
	char* if_addr;
	char* hostname;
	char* port;
	int family;
	int type;
	int protocol;
	int sockfd;
	char* hostip;
	int mid_protocol;
	long io_timeout;
	sigset_t* sigmask;

#ifdef LIBSSL_SANE
	SSL* ssl;
#endif

};

struct mid_client* sig_create_mid_client(struct mid_interface* mid_if, struct parsed_url* purl, sigset_t* sigmask);

int init_mid_client(struct mid_client* mid_cli);

int close_mid_client(struct mid_client* mid_cli);

int free_mid_client(struct mid_client** mid_cli);

void mid_protocol_quit(struct mid_client* mid_cli);

int mid_socket_write(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status);

int mid_socket_read(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status);

#define create_mid_client(var0, var1) sig_create_mid_client(var0, var1, NULL)

#endif /* MID_SOCKET_H_ */
