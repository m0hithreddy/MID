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
#include<openssl/ssl.h>
#include<openssl/err.h>
#include<netdb.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<limits.h>

struct network_data* create_http_request(struct http_request* s_request)
{
	if(s_request==NULL)
		return NULL;

	struct network_data* buf=(struct network_data*)malloc(sizeof(struct network_data));
	buf->data=malloc(HTTP_REQUEST_HEADERS_MAX_LEN*sizeof(char));

	struct data_bag *bag=create_data_bag();

	// Appending the start line and headers

	if(s_request->method!=NULL)
		buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s ",s_request->method);
	else
		buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s ",DEFAULT_HTTP_METHOD);

	place_data(bag,buf);

	if(s_request->path!=NULL)
		buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"/%s ",s_request->path);
	else
		buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s ",DEFAULT_HTTP_PATH);

	place_data(bag,buf);

	if(s_request->version!=NULL)
		buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"HTTP/%s\r\n",s_request->version);
	else
		buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"HTTP/%s\r\n",DEFAULT_HTTP_VERSION);

	place_data(bag,buf);

	if(s_request->host!=NULL)
	{
		if(s_request->port==NULL)
			buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"Host: %s\r\n",s_request->host);
		else
			buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"Host: %s:%s\r\n",s_request->host,s_request->port);

		place_data(bag,buf);
	}

	//Appending Remaining headers

	struct network_data* n_buf=(struct network_data*)malloc(sizeof(struct network_data));

	char* index_buffer;
	char* token_buffer;

	char* rqst_hdrs=(char*)malloc(sizeof(char)*(strlen(HTTP_REQUEST_HEADERS)+1));

	memcpy(rqst_hdrs,HTTP_REQUEST_HEADERS,strlen(HTTP_REQUEST_HEADERS)+1);

	n_buf->data=rqst_hdrs;
	n_buf->len=strlen(n_buf->data);

	void* v_s_request=s_request;

	while(1)
	{
		n_buf=sseek(n_buf," ");

		if(n_buf==NULL ||n_buf->data==NULL||n_buf->len==0)
			break;

		n_buf=scopy(n_buf," ",&token_buffer,-1);

		n_buf=sseek(n_buf," ");
		if(n_buf==NULL ||n_buf->data==NULL||n_buf->len==0)
			break;

		n_buf=scopy(n_buf," ",&index_buffer,-1);

		char** value=(char**)(v_s_request+(sizeof(char*)*(atoi(index_buffer)-1)));

		if(*value==NULL)
			continue;

		buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s: %s\r\n",token_buffer,*value);

		place_data(bag,buf);

	}


	// Appending custom headers

	if(s_request->custom_headers!=NULL)
	{
		for(int i=0;s_request->custom_headers[i]!=NULL;i++)
		{
			buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"%s: %s\r\n",s_request->custom_headers[i][0],s_request->custom_headers[i][1]);

			place_data(bag,buf);
		}
	}

	buf->len=snprintf(buf->data,HTTP_REQUEST_HEADERS_MAX_LEN,"\r\n");

	place_data(bag,buf);

	// Appending Body

	place_data(bag,s_request->body);

	// Returning Request

	return flatten_data_bag(bag);
}

struct http_response* parse_http_response(struct network_data *response)
{
	if(response==NULL)
		return NULL;

	struct http_response *s_response=(struct http_response*)calloc(1,sizeof(struct http_response));

	// Segregate Response Headers and Response Body

	struct network_data* hdrs=(struct network_data*)malloc(sizeof(struct network_data));

	char* current=strlocate(response->data,"\r\n\r\n",0,response->len);

	if(current==NULL)
		return NULL;

	hdrs->data=response->data;
	hdrs->len=(long)(current-(char*)response->data)+strlen("\r\n\r\n");

	s_response->body=(struct network_data*)malloc(sizeof(struct network_data));

	s_response->body->len=response->len-hdrs->len;
	s_response->body->data=(char*)malloc(sizeof(char)*(s_response->body->len));
	memcpy(s_response->body->data,response->data+hdrs->len,s_response->body->len);

	// Response Version

	hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return NULL;

	current=strlocate(hdrs->data,"HTTP/",0,response->len);

	if(current==NULL)
		return NULL;

	hdrs->data=current+strlen("HTTP/");
	hdrs->len=hdrs->len-(long)(current-(char*)hdrs->data)-strlen("HTTP/");

	hdrs=scopy(hdrs,MID_HTTP_TOKEN_DELIMITERS,&s_response->version,-1);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return s_response;

	// Response Status Code

	hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return s_response;

	hdrs=scopy(hdrs,MID_HTTP_TOKEN_DELIMITERS,&s_response->status_code,-1);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return s_response;

	// Response Status

	hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return s_response;

	hdrs=scopy(hdrs,MID_HTTP_VALUE_DELIMITERS,&s_response->status,-1);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return s_response;

	// Headers Extraction

	struct data_bag *bag=create_data_bag(); /* bag[i]="header" bag[i+1]="value" where i is even*/

	struct network_data* n_buf=(struct network_data*)malloc(sizeof(struct network_data));

	char* token_buffer;
	char* value_buffer;
	char* index_buffer;

	void* v_s_response=s_response;

	while(1)
	{
		//Token Extraction

		hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS);

		if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
			break;

		hdrs=scopy(hdrs,MID_HTTP_TOKEN_DELIMITERS,&token_buffer,-1);

		if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
			break;

		//Value Extraction

		hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS);

		if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
			break;

		hdrs=scopy(hdrs,MID_HTTP_VALUE_DELIMITERS,&value_buffer,-1);

		if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
			break;

		//Inserting the token-value pair

		char* comp_buffer=(char*)malloc(sizeof(char)*(strlen(token_buffer)+3));

		comp_buffer[0]=' ';
		memcpy(comp_buffer+1,token_buffer,strlen(token_buffer));
		comp_buffer[strlen(token_buffer)+1]=' ';
		comp_buffer[strlen(token_buffer)+2]='\0';

		if((current=strcaselocate(HTTP_RESPONSE_HEADERS,comp_buffer,0,strlen(HTTP_RESPONSE_HEADERS)))!=NULL)
		{
			n_buf->data=current+strlen(token_buffer)+2;
			n_buf->len=strlen(current);

			n_buf=sseek(n_buf," ");

			if(n_buf==NULL || n_buf->data==NULL || n_buf->len==0)
				continue;

			scopy(n_buf," ",&index_buffer,-1);

			char** header_token=(char**)(v_s_response+(sizeof(char*)*(atoi(index_buffer)-1)));

			*header_token=(char*)malloc(sizeof(char)*(strlen(value_buffer)+1));

			memcpy(*header_token,value_buffer,strlen(value_buffer)+1);

			continue;
		}

		n_buf->data=token_buffer;
		n_buf->len=strlen(token_buffer)+1;

		place_data(bag,n_buf);

		n_buf->data=value_buffer;
		n_buf->len=strlen(value_buffer)+1;

		place_data(bag,n_buf);
	}

	int cus_size=(bag->n_pockets/2)+(bag->n_pockets%2)+1;

	s_response->custom_headers=(char***)malloc(sizeof(char**)*cus_size);

	struct data_pocket *pocket=bag->first;

	for(int i=0;i<cus_size-1;i++)
	{
		s_response->custom_headers[i]=(char**)malloc(sizeof(char*)*2);

		s_response->custom_headers[i][0]=(char*)malloc(sizeof(char)*pocket->len);
		memcpy(s_response->custom_headers[i][0],pocket->data,pocket->len);

		s_response->custom_headers[i][1]=(char*)malloc(sizeof(char)*pocket->len);

		pocket=pocket->next;

		if(pocket==NULL)
		{
			s_response->custom_headers[i][1]=NULL;
			break;
		}

		s_response->custom_headers[i][1]=(char*)malloc(sizeof(char)*pocket->len);
		memcpy(s_response->custom_headers[i][1],pocket->data,pocket->len);

		pocket=pocket->next;

	}

	s_response->custom_headers[cus_size-1]=NULL;

	return s_response;

}

void* send_http_request(int sockfd,struct network_data* request,int flag)
{
	if(request==NULL)
		return NULL;

	if(sockfd<0)
		return NULL;

	long* status=(long*)malloc(sizeof(long));

	if(flag==0 || flag==JUST_SEND || flag==SEND_RECEIVE)
	{
		if(request->len!=(*status=sock_write(sockfd,request)))
		{
			return (void*)status;
		}
	}

	if(flag==JUST_SEND)
	{
		return (void*)status;
	}

	if(flag==0 || flag==JUST_RECEIVE || flag==SEND_RECEIVE)
	{
		struct network_data* response=sock_read(sockfd,LONG_MAX);

		return (void*)response;
	}

	return NULL;
}

void* send_https_request(int sockfd,struct network_data* request,int flag)
{

	if(sockfd<0)
		return NULL;

	SSL* ssl=ssl_open_connection(sockfd);

	if(ssl==NULL)
		return 0;

	if(ssl_sock_write(ssl,request)!=request->len)
	{
		return NULL;
	}

	if(flag==JUST_SEND)
	{
		return (void*)ssl;
	}

	struct network_data* response=ssl_sock_read(ssl);

	return (void*)response;
}

void* follow_redirects(struct http_request* c_s_request,struct network_data* response,long max_redirects,struct socket_info* cli_info,int flag)
{

	if(response==NULL)
		return NULL;

	struct http_response* s_response;
	struct parsed_url* purl;
	struct network_data* request=create_http_request(c_s_request);
	struct http_request* s_request=(struct http_request*)malloc(sizeof(struct http_request));

	memcpy(s_request,c_s_request,sizeof(struct http_request));

	long redirect_count=0;

	while(redirect_count<max_redirects)
	{
		s_response=parse_http_response(response);

		if(s_response->status_code[0]!='3' || s_response->location==NULL)
		{
			break;
		}

		if(args->vverbose_flag && !args->quiet_flag)
		{
			fprintf(stderr,"\nHTTP Redirection Number %ld:\n\n",redirect_count+1);
		}

		//Parse the URL

		purl=parse_url(s_response->location);

		if( purl==NULL || !( !strcmp(purl->scheme,"https") || !strcmp(purl->scheme,"http") ) )
		{
			return NULL;
		}

		if(args->vverbose_flag && !args->quiet_flag)
		{
			fprintf(stderr,"\n->URL Information:\n\n");

			fprintf(stderr,"-->Scheme: %s\n",purl->scheme);
			fprintf(stderr,"-->Host: %s\n",purl->host);
			fprintf(stderr,"-->Port: %s\n",purl->port);
			fprintf(stderr,"-->PATH: %s\n",purl->path);
			fprintf(stderr,"-->Query: %s\n",purl->query);
		}

		// DNS Query

		char* hostip=resolve_dns(purl->host);

		if(hostip==NULL)
			return NULL;

		if(args->vverbose_flag && !args->quiet_flag)
		{
			fprintf(stderr,"\n->DNS Resolve:\n\n");
			fprintf(stderr,"-->%s IPV4 address: %s\n",purl->host,hostip);
		}

		struct sockaddr *servaddr=create_sockaddr_in(hostip,atoi((purl->port!=NULL)? purl->port:(!(strcmp(purl->scheme,"http"))? DEFAULT_HTTP_PORT:DEFAULT_HTTPS_PORT)),AF_INET);

		if(servaddr==NULL)
		{
			return NULL;
		}

		s_request->host=purl->host;
		char* tmp_path=(char*)malloc(sizeof(char)*HTTP_REQUEST_HEADERS_MAX_LEN);
		strcpy(tmp_path,purl->path);
		if(purl->query!=NULL)
		{
			strcat(tmp_path,"?");
			strcat(tmp_path,purl->query);
		}
		s_request->path=tmp_path;
		s_request->url=s_response->location;
		s_request->hostip=hostip;

		request=create_http_request(s_request);

		if(args->vverbose_flag && !args->quiet_flag)
		{
			fprintf(stderr,"\n->HTTP Request Message:\n\n");
			fprintf(stderr,"-->");
			sock_write(fileno(stderr),request);
		}

		if(cli_info==NULL)
		{
			cli_info=(struct socket_info*)malloc(sizeof(struct socket_info));
			cli_info->address=NULL;
			cli_info->port=0;
			cli_info->family=DEFAULT_HTTP_SOCKET_FAMILY;
			cli_info->type=DEFAULT_HTTP_SOCKET_TYPE;
			cli_info->protocol=DEFAULT_HTTP_SOCKET_PROTOCOL;
		}

		int sockfd = open_connection(cli_info,servaddr);

		if(sockfd<0)
			return NULL;

		if(!strcmp(purl->scheme,"http"))
		{
			response=(struct network_data*)send_http_request(sockfd,request,0);
		}
		else
		{
			response=(struct network_data*)send_https_request(sockfd,request,0);
		}

		if(args->vverbose_flag && !args->quiet_flag)
		{
			fprintf(stderr,"\n->HTTP Response Message:\n\n");
			fprintf(stderr,"-->");
			sock_write(fileno(stderr),response);
		}

		redirect_count++;
	}


	if(flag==RETURN_RESPONSE)
		return (void*)response;

	if(flag==RETURN_S_RESPONSE)
		return (void*)parse_http_response(response);

	if(flag==RETURN_REQUEST)
		return (void*)request;

	if(flag==RETURN_S_REQUEST)
		return (void*)s_request;

	if(flag==RETURN_S_REQUEST_S_RESPONSE)
	{
		s_response=parse_http_response(response);

		if(s_response==NULL)
			return NULL;

		void* s_rqst_s_resp=(void*)malloc(sizeof(struct http_request)+sizeof(struct http_response));
		memcpy(s_rqst_s_resp,s_request,sizeof(struct http_request));
		memcpy(s_rqst_s_resp+sizeof(struct http_request),s_response,sizeof(struct http_response));
		return s_rqst_s_resp;
	}

	return NULL;
}

char* determine_filename(char* path)
{
	if(path==NULL)
		return determine_filename("index.html");

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

	char pre_name[strlen(prev)+1];

	memcpy(pre_name,prev,sizeof(pre_name));

	long counter=1;
	FILE* fp;
	char* fin_name=(char*)malloc(sizeof(char)*(sizeof(pre_name)+64+2));
	strcpy(fin_name,pre_name);

	while(1)
	{
		fp=fopen(fin_name,"r");

		if(fp==NULL)
			return fin_name;

		fclose(fp);

		sprintf(fin_name,"%s(%ld)",pre_name,counter);
		counter++;
	}

	return fin_name;
}

void handle_identity_encoding(struct encoding_info* en_info)
{

	long copy_size = en_info->out_max < en_info->in_max - en_info->in_len ? en_info->out_max : en_info->in_max - en_info->in_len;

	memcpy(en_info->out,en_info->in+en_info->in_len,copy_size);

	en_info->out_len=copy_size;
	en_info->in_len=en_info->in_len + copy_size;

}

void handle_chunked_encoding(struct encoding_info* en_info)
{

}

void handle_encodings(struct encoding_info* en_info)
{
	if(en_info->encoding == IDENTITY_ENCODING )
	{
		handle_identity_encoding(en_info);
	}
	else if(en_info->encoding == CHUNKED_ENCODING )
	{
		handle_chunked_encoding(en_info);
	}
}

struct encoding_info* determine_transfer_encodings(struct http_response* s_response)
{
	if(s_response == NULL)
	{
		return NULL;
	}

	if(s_response->transfer_encoding == NULL || !strcmp(s_response->transfer_encoding,"identity") )
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

	else if(!strcmp(s_response->transfer_encoding,"chunked"))
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

	return NULL; // Encoding not known or not handled
}



















