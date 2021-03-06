/*
 * MID_http.c
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#include"MID.h"
#include"MID_http.h"
#include"MID_structures.h"
#include"MID_functions.h"
#include"MID_socket.h"
#include"MID_ssl_socket.h"
#include"url_parser.h"
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<netdb.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<limits.h>
#include<zlib.h>
#include<sys/file.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

struct mid_data* create_http_request(struct http_request* s_request)
{
	if(s_request==NULL)
		return NULL;

	struct mid_data* rqst_buf=(struct mid_data*)malloc(sizeof(struct mid_data));
	rqst_buf->data=malloc(HTTP_REQUEST_HEADERS_MAX_LEN*sizeof(char));

	struct mid_bag *rqst_bag=create_mid_bag();

	/* Append the request line */

	if(s_request->method!=NULL)  // add the HTTP method
		rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s ",s_request->method);
	else
		rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s ",DEFAULT_HTTP_METHOD);

	place_mid_data(rqst_bag,rqst_buf);

	if(s_request->path!=NULL)  // add the resource path
		rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"/%s ",s_request->path);
	else
		rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s ",DEFAULT_HTTP_PATH);

	place_mid_data(rqst_bag,rqst_buf);

	if(s_request->version!=NULL) // add the HTTP version
		rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"HTTP/%s\r\n",s_request->version);
	else
		rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"HTTP/%s\r\n",DEFAULT_HTTP_VERSION);

	place_mid_data(rqst_bag,rqst_buf);

	if(s_request->host!=NULL)  // add the Host header, it requires special handling.
	{
		if(s_request->port==NULL)
			rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"Host: %s\r\n",s_request->host);
		else
			rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"Host: %s:%s\r\n",s_request->host,s_request->port);

		place_mid_data(rqst_bag,rqst_buf);
	}

	/* Appending remaining headers of the structure if they are set */

	struct mid_data* rqst_data=(struct mid_data*)malloc(sizeof(struct mid_data));

	char* index_buffer;
	char* token_buffer;

	char* rqst_hdrs=(char*)memndup(HTTP_REQUEST_HEADERS,sizeof(char)*(strlen(HTTP_REQUEST_HEADERS)+1));  /* HTTP_REQUEST_HEADERS contains entries likewise.

	"Header-Token i" Header-Token is the corresponding HTTP request header key field for the i^th entry of the http_request structure */

	rqst_data->data=rqst_hdrs;
	rqst_data->len=strlen(rqst_data->data);

	void* v_s_request=s_request;

	for( ; ; ) // Iterate through all the Header-Token's and find whether the corresponding index value is set.
	{
		rqst_data=sseek(rqst_data," ",-1,MID_PERMIT);

		if(rqst_data==NULL ||rqst_data->data==NULL||rqst_data->len==0) // End of the tokens, break.
			break;

		rqst_data=scopy(rqst_data," ",&token_buffer,-1,MID_DELIMIT);  // read the Header-Token

		rqst_data=sseek(rqst_data," ",-1,MID_PERMIT);
		if(rqst_data==NULL ||rqst_data->data==NULL||rqst_data->len==0)  // Should not happen in general, but break anyway.
			break;

		rqst_data=scopy(rqst_data," ",&index_buffer,-1,MID_DELIMIT);  // read the corresponding index number in the http_request structure.

		char** value=(char**)(v_s_request+(sizeof(char*)*(atoi(index_buffer)-1)));  // calculate the address of the index. struct address + relative address.

		if(*value==NULL)
			continue;

		rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s: %s\r\n",token_buffer,*value);

		place_mid_data(rqst_bag,rqst_buf);  // append the header

	}

	/* If any custom headers are set append them */

	if(s_request->custom_headers!=NULL)
	{
		for(long i=0;s_request->custom_headers[i]!=NULL;i++)
		{
			rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s: %s\r\n",s_request->custom_headers[i][0],s_request->custom_headers[i][1]);

			place_mid_data(rqst_bag,rqst_buf);
		}
	}

	rqst_buf->len=snprintf(rqst_buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"\r\n");  // Append CRLF terminating sequence.

	place_mid_data(rqst_bag,rqst_buf);

	/* Appending Body */

	if(s_request->body!=NULL && s_request->body->data!=NULL && s_request->body->len!=0)
		place_mid_data(rqst_bag,s_request->body);

	/* Flatten the rqst_bag and return it ! */

	rqst_data=flatten_mid_bag(rqst_bag);
	free_mid_bag(rqst_bag);

	return rqst_data;
}

struct http_response* parse_http_response(struct mid_data *response)
{
	if(response==NULL || response->data==NULL || response->len==0)
		return NULL;

	struct http_response *s_response=(struct http_response*)calloc(1,sizeof(struct http_response));

	/* Segregate Response Headers and Response Body */

	struct mid_data* hdrs_data=(struct mid_data*)malloc(sizeof(struct mid_data));

	char* current=strlocate(response->data,"\r\n\r\n",0,response->len-1);

	if(current==NULL)  // If \r\n\r\n not found, then not a valid HTTP response message.
		return NULL;

	hdrs_data->data=response->data;
	hdrs_data->len=(long)(current-(char*)response->data)+sizeof(char)*strlen("\r\n\r\n");   // Total length of the HTTP response headers (including \r\n\r\n).

	s_response->body=(struct mid_data*)malloc(sizeof(struct mid_data));
	s_response->body->len=response->len-hdrs_data->len;

	if(s_response->body->len!=0)
		s_response->body->data=(void*)memndup(response->data+hdrs_data->len,s_response->body->len);  // Duplicate the response body data into s_response->body
	else
		s_response->body->data=NULL;

	/* Few fields need special handling */

	hdrs_data=sseek(hdrs_data,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);   // Seek through ' ', : , \r , \n

	if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)
		return NULL;

	current=strlocate(hdrs_data->data,"HTTP/",0,response->len);

	if(current==NULL)  // If HTTP/ not found then return NULL
		return NULL;

	hdrs_data->data=current+sizeof(char)*strlen("HTTP/");   // Update hdrs_data->data and hdrs_data->len past the HTTP/
	hdrs_data->len=hdrs_data->len-(long)(current-(char*)hdrs_data->data)-sizeof(char)*strlen("HTTP/");

	hdrs_data=scopy(hdrs_data,MID_HTTP_TOKEN_DELIMITERS,&s_response->version,-1,MID_DELIMIT);  // read the HTTP version into s_response->version

	if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)  // Should not happen, but let the caller take decision.
		return s_response;

	/* Repeat the same procedure as above for HTTP response status_code and status */

	hdrs_data=sseek(hdrs_data,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);

	if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)
		return s_response;

	hdrs_data=scopy(hdrs_data,MID_HTTP_TOKEN_DELIMITERS,&s_response->status_code,-1,MID_DELIMIT);  // Response status_code.

	if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)
		return s_response;

	hdrs_data=sseek(hdrs_data,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);

	if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)
		return s_response;

	hdrs_data=scopy(hdrs_data,MID_HTTP_VALUE_DELIMITERS,&s_response->status,-1,MID_DELIMIT);  // Response status.

	if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)
		return s_response;

	/* Headers Extraction */

	struct mid_bag *cus_hdrs_bag=create_mid_bag(); /* hdrs_bag[i]="header" hdrs_bag[i+1]="value", i is even*/

	struct mid_data* tmp_data=(struct mid_data*)malloc(sizeof(struct mid_data));

	char* token_buffer;
	char* value_buffer;
	char* index_buffer;

	void* v_s_response=s_response;

	while(1)
	{
		// HTTP header format -> Token : Value

		hdrs_data=sseek(hdrs_data,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);

		if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)
			break;

		hdrs_data=scopy(hdrs_data,MID_HTTP_TOKEN_DELIMITERS,&token_buffer,-1,MID_DELIMIT);   // extract token.

		if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)
			break;

		hdrs_data=sseek(hdrs_data,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);

		if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)
			break;

		hdrs_data=scopy(hdrs_data,MID_HTTP_VALUE_DELIMITERS,&value_buffer,-1,MID_DELIMIT);  // extract value.

		if(hdrs_data==NULL || hdrs_data->data==NULL || hdrs_data->len==0)
			break;

		// Inserting the token-value pair either in s_response if entry is there or else in custom_header matrix.

		char* comp_buffer=(char*)malloc(sizeof(char)*(strlen(token_buffer)+3));

		comp_buffer[0]=' ';
		memcpy(comp_buffer+1,token_buffer,strlen(token_buffer));  /* Token is made " Token " , for the purpose of searching in HTTP_RESPONSE_HEADERS.

		HTTP_RESPONSE_HEADERS be like this "TOKEN i" , where TOKEN is HTTP header Token and i is the corresponding index in the s_response structure
		(used to compute the absolute address). " Token " is used to prevent any substring matches */

		comp_buffer[strlen(token_buffer)+1]=' ';
		comp_buffer[strlen(token_buffer)+2]='\0';

		if((current=strcaselocate(HTTP_RESPONSE_HEADERS,comp_buffer,0,strlen(HTTP_RESPONSE_HEADERS)))!=NULL) // If found in the default headers string then insert in s_response
		{
			tmp_data->data=1+current+sizeof(char)*strlen(token_buffer)+1;
			tmp_data->len=strlen(current);

			tmp_data=sseek(tmp_data," ",-1,MID_PERMIT);

			if(tmp_data==NULL || tmp_data->data==NULL || tmp_data->len==0)
				continue;

			scopy(tmp_data," ",&index_buffer,-1,MID_DELIMIT);  // index number

			char** header_token=(char**)(v_s_response+(sizeof(char*)*(atoi(index_buffer)-1)));  // absolute address of the token

			*header_token=(char*)memndup(value_buffer,sizeof(char)*(strlen(value_buffer)+1));

			continue;
		}

		// Else should be placed in the custom headers matrix

		tmp_data->data=token_buffer;
		tmp_data->len=strlen(token_buffer)+1;

		place_mid_data(cus_hdrs_bag,tmp_data);  // place token

		tmp_data->data=value_buffer;
		tmp_data->len=strlen(value_buffer)+1;

		place_mid_data(cus_hdrs_bag,tmp_data);  // place header.
	}

	/* Fill the custom headers matrix */

	long cus_size=(cus_hdrs_bag->n_pockets/2)+(cus_hdrs_bag->n_pockets%2)+1;   // +1 for the NULL at the end.

	s_response->custom_headers=(char***)malloc(sizeof(char**)*cus_size);

	struct mid_pocket *pocket=cus_hdrs_bag->first;

	for(long i=0;i<cus_size-1;i++)
	{
		s_response->custom_headers[i]=(char**)malloc(sizeof(char*)*2);

		s_response->custom_headers[i][0]=(char*)memndup(pocket->data,pocket->len);  // token

		pocket=pocket->next;

		if(pocket==NULL)  // Should not happen, but return;
		{
			s_response->custom_headers[i]=NULL;
			free_mid_bag(cus_hdrs_bag);

			return s_response;
		}

		s_response->custom_headers[i][1]=(char*)memndup(pocket->data,pocket->len);

		pocket=pocket->next;
	}

	s_response->custom_headers[cus_size-1]=NULL;
	free_mid_bag(cus_hdrs_bag);

	return s_response;
}

int mid_http(struct mid_client* mid_cli, struct mid_data* http_request, int http_flag, struct mid_bag* http_result)
{
	if (mid_cli == NULL || mid_cli->sockfd < 0
#ifdef LIBSSL_SANE
			|| (mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS && mid_cli->ssl == NULL)
#endif
			|| ((http_flag & (MID_MODE_READ_RESPONSE | MID_MODE_READ_HEADERS)) && http_result == NULL) || \
			((http_flag & MID_MODE_SEND_REQUEST) && (http_request == NULL || \
					http_request->data == NULL || http_request->len <= 0)))  {   // Invalid Input

		return MID_ERROR_INVAL;
	}

	/* Check if MID application protocol is  HTTP or HTTPS */

	if (mid_cli->mid_protocol != MID_CONSTANT_APPLICATION_PROTOCOL_HTTP
#ifdef LIBSSL_SANE
			 && mid_cli->mid_protocol != MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS
#endif
			 )  {

		return MID_ERROR_PROTOCOL_UNKOWN;
	}

	/* HTTP Request send procedure */

	if (http_flag & MID_MODE_SEND_REQUEST)
	{
		int wr_return;

		if (mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTP)  // For HTTP
			wr_return = mid_socket_write(mid_cli, http_request, MID_MODE_AUTO_RETRY, NULL);
#ifdef LIBSSL_SANE
		else if (mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS)  // For HTTPS.
			wr_return = mid_ssl_socket_write(mid_cli, http_request, MID_MODE_AUTO_RETRY, NULL);
#endif
		else   // just in case.
			return MID_ERROR_PROTOCOL_UNKOWN;

		if (wr_return != MID_ERROR_NONE)
			return wr_return;
	}

	/* HTTP Response Headers read procedure */

	if (http_flag & MID_MODE_READ_HEADERS)
	{
		struct mid_bag* hdr_bag = create_mid_bag();

		struct mid_data hdr_data;
		hdr_data.data = malloc(1);
		hdr_data.len = 1;

		int rd_return = MID_ERROR_NONE, term_seq = 0b0000;
		long rd_status = 0;

		/* Read one bit at a time. FIX ME :( */

		for ( ; ; )
		{
			rd_status = 0;

			if (mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTP)   // For HTTP case
				rd_return = mid_socket_read(mid_cli, &hdr_data, MID_MODE_AUTO_RETRY, &rd_status);
#ifdef LIBSSL_SANE
			else if (mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS)  // For HTTPS case
				rd_return = mid_ssl_socket_read(mid_cli, &hdr_data, MID_MODE_AUTO_RETRY, &rd_status);
#endif
			else  // Just in case.
				return MID_ERROR_PROTOCOL_UNKOWN;

			if (rd_return != MID_ERROR_NONE && rd_return != MID_ERROR_RETRY && \
					rd_return != MID_ERROR_BUFFER_FULL)  {  // If fatal error reported by read procedure.

				return rd_return;
			}

			/* Search for HTTP headers terminating sequence "\r\n\r\n" . term_seq => 0b0000 => \n2\r2\n1\r1 */

			if (rd_status > 0)
			{
				place_mid_data(hdr_bag, &hdr_data);

				if (term_seq & 0b0001)
				{
					if (term_seq & 0b0010)
					{
						if (term_seq & 0b0100)
						{
							if (((char*) hdr_data.data)[0] == '\n')   // If terminating sequence found.
								break;
							else
								term_seq = 0b0000;
						}
						else if (((char*) hdr_data.data)[0] == '\r')
							term_seq |= 0b0100;
						else
							term_seq = 0b0000;
					}
					else if (((char*) hdr_data.data)[0] == '\n')
						term_seq |= 0b0010;
					else
						term_seq = 0b0000;
				}
				else if (((char*) hdr_data.data)[0] == '\r')
					term_seq |= 0b0001;
			}

			if (rd_return == MID_ERROR_NONE)
				break;
		}

		/* Append response headers to http_result */

		hdr_data.data = (void*) flatten_mid_bag(hdr_bag);
		hdr_data.len = sizeof(struct mid_data);

		place_mid_data(http_result, &hdr_data);
	}

	/* HTTP Response read procedure */

	if (http_flag & MID_MODE_READ_RESPONSE)
	{
		struct mid_bag* rsp_bag = create_mid_bag();

		struct mid_data rsp_data;
		rsp_data.data = malloc(MAX_TRANSACTION_SIZE);
		rsp_data.len = MAX_TRANSACTION_SIZE;

		int rd_return = MID_ERROR_NONE;
		long rd_status = 0;

		/* Read till server terminates connection */

		for ( ; ; )
		{
			rd_status = 0;
			rsp_data.len = MAX_TRANSACTION_SIZE;

			if (mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTP)  // For HTTP
				rd_return = mid_socket_read(mid_cli, &rsp_data, MID_MODE_AUTO_RETRY, &rd_status);
#ifdef LIBSSL_SANE
			else if (mid_cli->mid_protocol == MID_CONSTANT_APPLICATION_PROTOCOL_HTTPS)  // For HTTPS
				rd_return = mid_ssl_socket_read(mid_cli, &rsp_data, MID_MODE_AUTO_RETRY, &rd_status);
#endif
			else  // Just in case.
				return MID_ERROR_PROTOCOL_UNKOWN;

			if (rd_return != MID_ERROR_NONE || rd_return != MID_ERROR_RETRY || \
					rd_return != MID_ERROR_BUFFER_FULL)  {  // Fatal error reported by read operation.

				return rd_return;
			}

			if (rd_status > 0)
			{
				rsp_data.len = rd_status;
				place_mid_data(rsp_bag, &rsp_data);
			}

			if (rd_return == MID_ERROR_NONE)
				break;
		}

		/* Append Response data to http_result */

		rsp_data.data = (void*) flatten_mid_bag(rsp_bag);
		rsp_data.len = sizeof(struct mid_data);

		place_mid_data(http_result, &rsp_data);
	}

	return MID_ERROR_NONE;
}

int sig_follow_redirects(struct http_request* c_s_request, struct mid_data* c_response, struct mid_interface* mid_if, \
		long max_redirects, int fr_flag, struct mid_bag* fr_result, sigset_t* sigmask)  {

	if (c_s_request == NULL || c_response == NULL || c_response->data == NULL || \
			c_response->len <= 0 || fr_result == NULL) {  // Invalid input.

		return MID_ERROR_INVAL;
	}

	/* Request variables initializations */

	struct mid_data* request = NULL;
	struct http_request* s_request = (struct http_request*) memndup(c_s_request, sizeof(struct http_request));

	/* Response variables initializations */

	struct mid_data* response = (struct mid_data*) memndup(c_response, sizeof(struct mid_data));
	struct http_response* s_response = NULL;

	/* Follow redirects */

	struct parsed_url* purl = NULL;
	int in_return = MID_ERROR_NONE, http_return = MID_ERROR_NONE;
	long redirect_count = 0;

	for ( ; redirect_count < max_redirects; redirect_count++)
	{
		/* Parse the URL and check whether redirection can be followed */

		s_response = parse_http_response(response);

		if (s_response == NULL)  // Failed to parse HTTP response.
		{
			return MID_ERROR_FATAL;
		}

		if (s_response->status_code[0] != '3' || s_response->location == NULL)  {  /* If status_code is not 3xx ||
		location is not set then break */

			break;
		}

		purl = parse_url(s_response->location); // Parse the new file location (new URL).

		if (purl == NULL || (strcmp(purl->scheme,"http") != 0 && \
				strcmp(purl->scheme, "https") != 0)) {  /* If the redirection led to different scheme than HTTP[S] */

			return MID_ERROR_FATAL;
		}

		/* Setup sockets [and SSL] */

		struct mid_client* mid_cli = sig_create_mid_client(mid_if, purl, sigmask);

		in_return = init_mid_client(mid_cli);

		if (in_return != MID_ERROR_NONE)
			return in_return;

		/* Update s_request{} fields */

		s_request->host = purl->host;  // Host-Name

		if (purl->path != NULL)  // Set path and query
		{
			struct mid_bag* pq_bag = create_mid_bag();
			struct mid_data pq_data;

			/* Append Path */

			pq_data.data = purl->path;
			pq_data.len = strlen(purl->path);
			place_mid_data(pq_bag, &pq_data);

			if (purl->query != NULL)  // If query is set.
			{
				/* Append "?" */

				pq_data.data = "?";
				pq_data.len = 1;
				place_mid_data(pq_bag, &pq_data);

				/* Append query */

				pq_data.data = purl->query;
				pq_data.len = strlen(purl->query);
				place_mid_data(pq_bag, &pq_data);
			}

			/* Append "\0" */

			pq_data.data = "\0";
			pq_data.len = 1;
			place_mid_data(pq_bag, &pq_data);

			s_request->path = (char*) (flatten_mid_bag(pq_bag)->data);
		}
		else
			s_request->path = NULL;

		/* Update Misc entries of s_request{} */

		s_request->url = s_response->location;
		s_request->scheme = purl->scheme;
		s_request->hostip = mid_cli->hostip;

		/* Create and Send HTTP[S] Request */

		request = create_http_request(s_request);

		struct mid_bag* http_result = create_mid_bag();

			/* If asked to follow headers then just read headers, else read whole response */

		http_return = mid_http(mid_cli, request, MID_MODE_SEND_REQUEST | ((fr_flag & MID_MODE_FOLLOW_HEADERS) ? \
				MID_MODE_READ_HEADERS : MID_MODE_READ_RESPONSE), http_result);

		free_mid_client(&mid_cli);

		if(http_return != MID_ERROR_NONE)
			return http_return;

		response = (struct mid_data*) http_result->end->data;
	}

	/* Compute Final results */

	request = create_http_request(s_request);
	s_response = parse_http_response(response);

	if (request == NULL || s_request == NULL || response == NULL || s_response == NULL) // Just in case.
		return MID_ERROR_FATAL;

	/* Set results */

	struct mid_data rs_data;

	if (fr_flag & MID_MODE_RETURN_REQUEST)   // If requested for HTTP Request.
	{
		rs_data.data = (void*) request;
		rs_data.len = sizeof(struct mid_data);

		place_mid_data(fr_result, &rs_data);
	}

	if (fr_flag & MID_MODE_RETURN_S_REQUEST)  // If requested for s_request{}.
	{
		rs_data.data = (void*) s_request;
		rs_data.len = sizeof(struct http_request);

		place_mid_data(fr_result, &rs_data);
	}

	if (fr_flag & MID_MODE_RETURN_RESPONSE)  // If requested for HTTP response.
	{
		rs_data.data = (void*) response;
		rs_data.len = sizeof(struct mid_data);

		place_mid_data(fr_result, &rs_data);
	}

	if (fr_flag & MID_MODE_RETURN_S_RESPONSE) // If requested for s_response{}.
	{
		rs_data.data = (void*) s_response;
		rs_data.len = sizeof(struct http_response);

		place_mid_data(fr_result, &rs_data);
	}

	return MID_ERROR_NONE;
}

char* determine_filename(char* pre_name,FILE** fp_ptr)
{

	char* fin_name=(char*)malloc(sizeof(char)*(strlen(pre_name)+64+3));
	strcpy(fin_name,pre_name);

	long counter=1;
	FILE* fp;

	// Check whether file is already there or not

	while(1)
	{
		fp=fopen(fin_name,"r");

		if(fp==NULL)
		{
			*fp_ptr=fopen(fin_name,"a+");

			if(*fp_ptr==NULL)
				goto skip_if;

			fclose(*fp_ptr);

			*fp_ptr=fopen(fin_name,"r+");

			if(*fp_ptr==NULL)
				goto skip_if;

			if(flock(fileno(*fp_ptr),LOCK_EX | LOCK_NB)!=0)
			{
				fclose(*fp_ptr);
				goto skip_if;
			}

			return fin_name;
		}

		skip_if:

		if(fp!=NULL)
			fclose(fp);

		sprintf(fin_name,"%s(%ld)",pre_name,counter);
		counter++;
	}

	return fin_name;
}

char* path_to_filename(char* path,FILE** fp_ptr)
{
	if(path==NULL || strlen(path)==0) //Default file
		return determine_filename("index.html",fp_ptr);

	char* current=path;
	char* prev=current;

	while(1)
	{
		current=strlocate(current,"/",0,strlen(current));
		if(current==NULL)
			break;
		current=current+1;
		prev=current;
	}

	if(strlen(prev)==0)
		return determine_filename("index.html",fp_ptr);

	return determine_filename(prev,fp_ptr);
}

char* url_to_filename(char* url)
{
	char* file=strdup(url);

	for(long i=0;i<strlen(file);i++)
	{
		if(file[i]=='/')
			file[i]='_';
	}

	return file;
}

int handle_identity_encoding(struct encoding_info* en_info)
{

	long copy_size = en_info->out_max < en_info->in_max - en_info->in_len ? en_info->out_max : en_info->in_max - en_info->in_len;

	memcpy(en_info->out,en_info->in+en_info->in_len,copy_size);

	en_info->out_len=copy_size;
	en_info->in_len=en_info->in_len + copy_size;

	return EN_OK;

}

int handle_chunked_encoding(struct encoding_info* en_info)
{
	if(en_info->data==NULL) // Encoding state information initialization
	{
		en_info->data=calloc(1,sizeof(long)+23*sizeof(char)+sizeof(long)+5*sizeof(int)); // rem_chunk_len + chunk_len_str + str_counter + chunk_len + r1_flag + n1_flag + r2_flag + n2_flag

		((char*)(en_info->data+sizeof(long)))[0]='0';
		((char*)(en_info->data+sizeof(long)))[1]='X';
		*((int*)(en_info->data+sizeof(long)+23*sizeof(char)))=2;

	}

	long* rem_chunk_len=(long*)en_info->data;
	char* chunk_len_str=(char*)(en_info->data+sizeof(long));
	int* str_counter=(int*)(en_info->data+sizeof(long)+23*sizeof(char));
	long* chunk_len=(long*)(en_info->data+sizeof(long)+23*sizeof(char)+sizeof(int));
	int* r1_flag=(int*)(en_info->data+sizeof(long)+23*sizeof(char)+sizeof(int)+sizeof(long));
	int* n1_flag=(int*)(en_info->data+sizeof(long)+23*sizeof(char)+sizeof(int)+sizeof(long)+sizeof(int));
	int* r2_flag=(int*)(en_info->data+sizeof(long)+23*sizeof(char)+sizeof(int)+sizeof(long)+2*sizeof(int));
	int* n2_flag=(int*)(en_info->data+sizeof(long)+23*sizeof(char)+sizeof(int)+sizeof(long)+3*sizeof(int));

	long in_counter=en_info->in_len;
	en_info->out_len=0;

	if(*r1_flag!=1) // Chunk size in hexadecimal till the first \r is encountered
	{
		while(en_info->in_len!=en_info->in_max)
		{
			if( !( ((char*)en_info->in)[in_counter]=='\r' || ( ((char*)en_info->in)[in_counter] >= '0' && ((char*)en_info->in)[in_counter] <= '9' ) || ( ((char*)en_info->in)[in_counter] >= 'a' && ((char*)en_info->in)[in_counter] <= 'f' ) || ( ((char*)en_info->in)[in_counter] >= 'A' && ((char*)en_info->in)[in_counter] <= 'F' ) ) )
			{
				return EN_ERROR;
			}

			if(((char*)en_info->in)[in_counter]=='\r') // \r found break
			{
				*r1_flag=1;
				in_counter++;
				en_info->in_len=en_info->in_len+1;

				break;
			}

			((char*)chunk_len_str)[*str_counter]=((char*)en_info->in)[in_counter];
			*str_counter=*str_counter+1;
			in_counter++;
			en_info->in_len=en_info->in_len+1;
		}
	}

	if(en_info->in_len==en_info->in_max)
		return EN_OK;

	if(*n1_flag!=1) // \n follows after \r
	{
		if(((char*)en_info->in)[in_counter]!='\n')
			return EN_ERROR;

		*chunk_len=strtol(chunk_len_str,NULL,16);
		*rem_chunk_len=*chunk_len;

		*n1_flag=1;
		in_counter++;
		en_info->in_len=en_info->in_len+1;

	}

	if(en_info->in_len==en_info->in_max)
		return EN_OK;

	if(*rem_chunk_len!=0) // Chunk data
	{
		long copy_size;

		if(*rem_chunk_len < en_info->in_max-en_info->in_len)
			copy_size=*rem_chunk_len;
		else
			copy_size=en_info->in_max-en_info->in_len;

		if(en_info->out_max < copy_size)
			copy_size=en_info->out_max;

		memcpy(en_info->out,en_info->in+en_info->in_len,copy_size);

		en_info->out_len=copy_size;
		en_info->in_len=en_info->in_len+copy_size;
		*rem_chunk_len=*rem_chunk_len-copy_size;
		in_counter=in_counter+copy_size;
	}

	if(en_info->in_len==en_info->in_max)
		return EN_OK;

	if(*r2_flag!=1) // \r follows after chunk data
	{
		if(((char*)en_info->in)[in_counter]!='\r')
			return EN_ERROR;

		*r2_flag=1;
		in_counter++;
		en_info->in_len=en_info->in_len+1;
	}

	if(en_info->in_len==en_info->in_max)
		return EN_OK;

	if(*n2_flag!=1) // \n follows after \r
	{
		if(((char*)en_info->in)[in_counter]!='\n')
			return EN_ERROR;

		*n2_flag=1;
		in_counter++;
		en_info->in_len=en_info->in_len+1;
	}

	if(*chunk_len==0) // If chunk_len is zero then last chunk, decoding done.
		return EN_END;
	else // Make preparations to handle upcoming chunk
	{
		*r1_flag=0;
		*n1_flag=0;
		*r2_flag=0;
		*n2_flag=0;
		*str_counter=2;
		memset(chunk_len_str+*str_counter,0,21);
	}

	return EN_OK;

}

#ifdef LIBZ_SANE
int handle_gzipped_encoding(struct encoding_info* en_info)
{
	if(en_info->data==NULL)
	{
		en_info->data=calloc(1,sizeof(z_stream)+sizeof(int));

		((z_stream*)en_info->data)->zalloc = Z_NULL;
		((z_stream*)en_info->data)->zfree = Z_NULL;
		((z_stream*)en_info->data)->opaque = Z_NULL;
		((z_stream*)en_info->data)->avail_in = 0;
		((z_stream*)en_info->data)->next_in = Z_NULL;

		if(inflateInit2((z_stream*)en_info->data,MAX_WBITS+16)!=Z_OK)
			return EN_ERROR;

		*((int*)(en_info->data+sizeof(z_stream)))=0;
	}

	z_stream* strm=(z_stream*)(en_info->data);
	int* init_flag=(int*)(en_info->data+sizeof(z_stream));

	if(!(*init_flag))
	{
		strm->next_in = en_info->in;
		strm->avail_in = en_info->in_max;
		*init_flag=1;
	}

	strm->avail_out = en_info->out_max;
	strm->next_out = en_info->out;

	int ret = inflate(strm, Z_FINISH);

	if( ret == Z_DATA_ERROR || ret == Z_NEED_DICT || ret == Z_MEM_ERROR )
	{
		inflateEnd(strm);
		return EN_ERROR;
	}


	en_info->out_len = en_info->out_max - strm->avail_out;

	if(strm->avail_out!=0)
	{
		*init_flag=0;
		en_info->in_len=en_info->in_max;
	}

	if(ret==Z_STREAM_END)
	{
		inflateEnd(strm);
		return EN_END;
	}

	return EN_OK;
}
#endif

int handle_encodings(struct encoding_info* en_info) // Interface connecting actual decoding functions and callers
{
	if(en_info->encoding == IDENTITY_ENCODING )
	{
		return handle_identity_encoding(en_info);
	}
	else if(en_info->encoding == CHUNKED_ENCODING )
	{
		return handle_chunked_encoding(en_info);
	}
#ifdef LIBZ_SANE
	else if(en_info->encoding == GZIPPED_ENCODING)
	{
		return handle_gzipped_encoding(en_info);
	}
#endif

	return EN_UNKNOWN;
}

struct encoding_info* determine_encodings(char* encoding_str)
{

	if(encoding_str == NULL || !strcmp(encoding_str,"identity") )
	{
		struct encoding_info* en_info=(struct encoding_info*)calloc(1,sizeof(struct encoding_info));

		en_info->encoding=IDENTITY_ENCODING;
		en_info->in=NULL;
		en_info->in_len=0;
		en_info->in_max=0;
		en_info->out=malloc(IDENTITY_ENCODING_BUFFER_SIZE);
		en_info->out_len=0;
		en_info->out_max=IDENTITY_ENCODING_BUFFER_SIZE;
		en_info->data=NULL;

		return en_info;
	}

	else if(!strcmp(encoding_str,"chunked"))
	{
		struct encoding_info* en_info=(struct encoding_info*)calloc(1,sizeof(struct encoding_info));

		en_info->encoding=CHUNKED_ENCODING;
		en_info->in=NULL;
		en_info->in_len=0;
		en_info->in_max=0;
		en_info->out=malloc(CHUNKED_ENCODING_BUFFER_SIZE);
		en_info->out_len=0;
		en_info->out_max=CHUNKED_ENCODING_BUFFER_SIZE;
		en_info->data=NULL;

		return en_info;
	}

#ifdef LIBZ_SANE
	else if(!strcmp(encoding_str,"gzip"))
	{
		struct encoding_info* en_info=(struct encoding_info*)calloc(1,sizeof(struct encoding_info));

		if(en_info==NULL)
			return NULL;
		en_info->encoding=GZIPPED_ENCODING;
		en_info->in=NULL;
		en_info->in_len=0;
		en_info->in_max=0;
		en_info->out=malloc(GZIPPED_ENCODING_BUFFER_SIZE);
		en_info->out_len=0;
		en_info->out_max=GZIPPED_ENCODING_BUFFER_SIZE;
		en_info->data=NULL;

		return en_info;
	}
#endif

	return NULL; // Encoding not known or not handled
}
