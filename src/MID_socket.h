/*
 * MID_socket.h
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_SOCKET_H_
#define MID_SOCKET_H_

#define MAX_TRANSACTION_SIZE 65536
#define MAX_TCP_SYN_RETRANSMITS 2

#define MID_DEFAULT_IO_TIMEOUT 60

#define MID_CONSTANT_APPLICATION_PROTOCOL_HTTP 0
#define MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS 1
#define MID_CONSTANT_APPLICATION_PROTOCOL_UNKNOWN 2

#define MID_MODE_SOCK_WRITE_AUTO_RETRY 0
#define MID_MODE_SOCK_WRITE_PARTIAL_WRITE 1

#define MID_ERROR_SOCK_WRITE_NONE 0
#define MID_ERROR_SOCK_WRITE_RETRY 1
#define MID_ERROR_SOCK_WRITE_ERROR 2
#define MID_ERROR_SOCK_WRITE_INVAL 3
#define MID_ERROR_SOCK_WRITE_TIMEOUT 4

#define MID_MODE_SOCK_READ_AUTO_RETRY 0
#define MID_MODE_SOCK_READ_PARTIAL_READ 1

#define MID_ERROR_SOCK_READ_NONE 0
#define MID_ERROR_SOCK_READ_RETRY 1
#define MID_ERROR_SOCK_READ_ERROR 2
#define MID_ERROR_SOCK_READ_INVAL 3
#define MID_ERROR_SOCK_READ_TIMEOUT 4
#define MID_ERROR_SOCK_READ_BUFFER_FULL 5


#include"MID_structures.h"
#include"MID_interfaces.h"
#include"url_parser.h"
#include<netinet/in.h>
#include<sys/socket.h>

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

#ifdef LIBSSL_SANE
	SSL* ssl;
#endif

};

struct mid_client* create_mid_client(struct mid_interface* mid_if, struct parsed_url* purl);

int init_mid_client(struct mid_client* mid_cli);

void free_mid_client(struct mid_client* mid_cli);

int close_mid_client(struct mid_client* mid_cli);

int destroy_mid_client(struct mid_client* mid_cli);

void mid_protocol_quit(struct mid_client* mid_cli);

int mid_socket_write(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status);

int mid_socket_read(struct mid_client* mid_cli, struct mid_data* m_data, int mode, long* status);

#endif /* MID_SOCKET_H_ */
