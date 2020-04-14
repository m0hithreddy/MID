/*
 * MID_http.h
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_HTTP_H_
#define MID_HTTP_H_

#define HTTP_REQUEST_HEADERS_MAX_LEN 8192
#define HTTP_RESPONSE_HEADERS_MAX_LEN 8192
#define DEFAULT_HTTP_METHOD "GET"
#define DEFAULT_HTTP_PATH "/"
#define DEFAULT_HTTP_VERSION "1.0"
#define DEFAULT_HTTP_PORT "80"
#define DEFAULT_HTTPS_PORT "443"
#define DEFAULT_HTTP_SOCKET_FAMILY AF_INET
#define DEFAULT_HTTP_SOCKET_TYPE SOCK_STREAM
#define DEFAULT_HTTP_SOCKET_PROTOCOL IPPROTO_TCP
#define DEFAULT_MAX_HTTP_REDIRECTS 20
#define DEFAULT_MAX_MIRRORS 1
#define MID_HTTP_TOKEN_DELIMITERS "\r\n: "
#define MID_HTTP_VALUE_DELIMITERS "\r\n"
#define MAX_URL_SIZE 2000
#define SEND_RECEIVE 11
#define JUST_SEND 10
#define RETURN_RESPONSE 000
#define RETURN_S_RESPONSE 001
#define RETURN_REQUEST 010
#define RETURN_S_REQUEST 011
#define RETURN_S_REQUEST_S_RESPONSE 100
#define IDENTITY_ENCODING 00
#define CHUNKED_ENCODING 01
#define GZIPPED_ENCODING 10
#define IDENTITY_ENCODING_BUFFER_SIZE 65536
#define CHUNKED_ENCODING_BUFFER_SIZE 65536
#define GZIPPED_ENCODING_BUFFER_SIZE 65536*4
#define EN_OK 0
#define EN_ERROR -1
#define EN_UNKNOWN -2
#define EN_END 1

#define HTTP_REQUEST_HEADERS " User-Agent 6 Accept 7 Accept-Encoding 8 Connection 9 Content-Type 10 Content-Length 11 Accept-Language 12 Referer 13 Upgrade-Insecure-Requests 14 If-Modified-Since 15 If-None-Match 16 Cache-Control 17 Date 18 Pragma 19 Trailer 20 Transfer-Encoding 21 Upgrade 22 Via 23 Warning 24 Accept-Charset 25 Authorization 26 Expect 27 From 28 If-Match 29 If-Match 30 If-Unmodified-Since 31 Max-Forwards 32 Proxy-Authorization 33 Range 34 TE 35 " 

/* " header i " i^th position's Header Token in the structure (Note: Padding is not disabled, so group all header tokens(char*) at the start of the structure if adding new) */

#define HTTP_RESPONSE_HEADERS " DATE 4 CONTENT-TYPE 5 SERVER 6 ACCEPT-RANGES 7 VARY 8 CONNECTION 9 LOCATION 10 CONTENT-LENGTH 11 KEEP-ALIVE 12 ACCESS-CONTROL-ALLOW-ORIGIN 13 LAST-MODIFIED 14 CONTENT-ENCODING 15 TRANSFER-ENCODING 16 ALT-SVC 17 CACHE-CONTROL 18 PRAGMA 19 TRAILER 20 UPGRADE 21 VIA 22 WARNING 23 AGE 24 ETAG 25 PROXY-AUTHENTICATE 26 RETRY-AFTER 27 WWW-AUTHENTICATE 28 ALLOW 29 CONTENT-LANGUAGE 30 CONTENT-LOCATION 31 CONTENT-MD5 32 CONTENT-RANGE 33 EXPIRES 34 EXTENSION-HEADER 35 "

/* " header i " i^th position for header variable in the structure (Note: Padding is not disabled, so group all header tokens(char*) at the start of the structure if adding new) */

#include"MID_structures.h"
#include"MID_socket.h"
#include"url_parser.h"
#include<stdio.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

struct http_request
{
	char* method;
	char* path;
	char* version;
	char* host;
	char* port;
	char* user_agent;
	char* accept;
	char* accept_encoding;
	char* connection;
	char* content_type;
	char* content_length;
	char* accept_language;
	char* referer;
	char* upgrade_insecure_requests;
	char* if_modified_since;
	char* if_none_match;
	char* cache_control;
	char* date;
	char* pragma;
	char* trailer;
	char* transfer_encoding;
	char* upgrade;
	char* via;
	char* warning;
	char* accept_charset;
	char* authorization;
	char* expect;
	char* from;
	char* if_match;
	char* if_range;
	char* if_unmodified_since;
	char* max_forwards;
	char* proxy_authorization;
	char* range;
	char* te;
	char ***custom_headers; /* custom[i][0]="Header" custom[i][1]="Value" custom[end]==NULL (NULL terminating) */
	struct network_data* body;

	// Misc Entries
	char* url;
	char* hostip;
};

struct http_response
{
	char* version;
	char* status_code;
	char* status;
	char* date;
	char* content_type;
	char* server;
	char* accept_ranges;
	char* vary;
	char* connection;
	char* location;
	char* content_length;
	char* keep_alive;
	char* access_control_allow_orgin;
	char* last_modified;
	char* content_encoding;
	char* transfer_encoding;
	char* alt_svc;
	char* cache_control;
	char* pragma;
	char* trailer;
	char* upgrade;
	char* via;
	char* warning;
	char* age;
	char* etag;
	char* proxy_authenticate;
	char* retry_after;
	char* www_authenticate;
	char* allow;
	char* content_language;
	char* content_location;
	char* content_md5;
	char* content_range; 
	char* expires;
	char* extension_header;
	char ***custom_headers; /* custom[i][0]="Header" custom[i][1]="Value" custom[end]==NULL (NULL terminating) */
	char* url;
	struct network_data *body;
};

struct http_range
{
	long start;
	long end;
};

struct encoding_info
{
	int encoding; // Encoding used
	void* in; // In buffer
	long in_len;
	long in_max;
	void* out; // Out buffer
	long out_len;
	long out_max;
	void* data; // Encoding specific data used by function handling the encoding
};

struct network_data* create_http_request(struct http_request* s_request);

struct http_response* parse_http_response(struct network_data *response);

void* send_http_request(int sockfd,struct network_data* request,char* hostname,int flag);

void* send_https_request(int sockfd,struct network_data* request,char* hostname,int flag);

void* follow_redirects(struct http_request* c_s_request,struct network_data* response,long max_redirects,struct socket_info* cli_info,int flag);

char* determine_filename(char* path,FILE** fp_ptr);

char* path_to_filename(char* path,FILE** fp_ptr); // path is passed with out the beginning of '/'

char* url_to_filename(char* url);

int handle_identity_encoding(struct encoding_info* en_info);

int handle_chunked_encoding(struct encoding_info* en_info);

#ifdef LIBZ_SANE
int handle_gzipped_encoding(struct encoding_info* en_info);
#endif

int handle_encodings(struct encoding_info* en_info);

struct encoding_info* determine_encodings(char* encoding_str);

#ifndef LIBSSL_SANE
void https_quit();
#endif

#endif /* MID_HTTP_H_ */
