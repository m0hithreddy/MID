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

	char* rqst_hdrs=(char*)memndup(HTTP_REQUEST_HEADERS,sizeof(char)*(strlen(HTTP_REQUEST_HEADERS)+1));

	n_buf->data=rqst_hdrs;
	n_buf->len=strlen(n_buf->data);

	void* v_s_request=s_request;

	while(1)
	{
		n_buf=sseek(n_buf," ",-1,MID_PERMIT);

		if(n_buf==NULL ||n_buf->data==NULL||n_buf->len==0)
			break;

		n_buf=scopy(n_buf," ",&token_buffer,-1,MID_DELIMIT);

		n_buf=sseek(n_buf," ",-1,MID_PERMIT);
		if(n_buf==NULL ||n_buf->data==NULL||n_buf->len==0)
			break;

		n_buf=scopy(n_buf," ",&index_buffer,-1,MID_DELIMIT);

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
	s_response->body->data=(char*)memndup(response->data+hdrs->len,sizeof(char)*(s_response->body->len));

	// Response Version

	hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return NULL;

	current=strlocate(hdrs->data,"HTTP/",0,response->len);

	if(current==NULL)
		return NULL;

	hdrs->data=current+strlen("HTTP/");
	hdrs->len=hdrs->len-(long)(current-(char*)hdrs->data)-strlen("HTTP/");

	hdrs=scopy(hdrs,MID_HTTP_TOKEN_DELIMITERS,&s_response->version,-1,MID_DELIMIT);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return s_response;

	// Response Status Code

	hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return s_response;

	hdrs=scopy(hdrs,MID_HTTP_TOKEN_DELIMITERS,&s_response->status_code,-1,MID_DELIMIT);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return s_response;

	// Response Status

	hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);

	if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
		return s_response;

	hdrs=scopy(hdrs,MID_HTTP_VALUE_DELIMITERS,&s_response->status,-1,MID_DELIMIT);

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

		hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);

		if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
			break;

		hdrs=scopy(hdrs,MID_HTTP_TOKEN_DELIMITERS,&token_buffer,-1,MID_DELIMIT);

		if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
			break;

		//Value Extraction

		hdrs=sseek(hdrs,MID_HTTP_TOKEN_DELIMITERS,-1,MID_PERMIT);

		if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
			break;

		hdrs=scopy(hdrs,MID_HTTP_VALUE_DELIMITERS,&value_buffer,-1,MID_DELIMIT);

		if(hdrs==NULL || hdrs->data==NULL || hdrs->len==0)
			break;

		//Inserting the token-value pair

		char* comp_buffer=(char*)malloc(sizeof(char)*(strlen(token_buffer)+3));

		comp_buffer[0]=' ';
		memcpy(comp_buffer+1,token_buffer,strlen(token_buffer));
		comp_buffer[strlen(token_buffer)+1]=' ';
		comp_buffer[strlen(token_buffer)+2]='\0';

		if((current=strcaselocate(HTTP_RESPONSE_HEADERS,comp_buffer,0,strlen(HTTP_RESPONSE_HEADERS)))!=NULL) // If found in the default headers string
		{
			n_buf->data=current+strlen(token_buffer)+2;
			n_buf->len=strlen(current);

			n_buf=sseek(n_buf," ",-1,MID_PERMIT);

			if(n_buf==NULL || n_buf->data==NULL || n_buf->len==0)
				continue;

			scopy(n_buf," ",&index_buffer,-1,MID_DELIMIT);

			char** header_token=(char**)(v_s_response+(sizeof(char*)*(atoi(index_buffer)-1)));

			*header_token=(char*)memndup(value_buffer,sizeof(char)*(strlen(value_buffer)+1));

			continue;
		}

		// Else should be placed in the custom headers matrix

		n_buf->data=token_buffer;
		n_buf->len=strlen(token_buffer)+1;

		place_data(bag,n_buf);

		n_buf->data=value_buffer;
		n_buf->len=strlen(value_buffer)+1;

		place_data(bag,n_buf);
	}

	// Fill the custom headers matrix

	int cus_size=(bag->n_pockets/2)+(bag->n_pockets%2)+1;

	s_response->custom_headers=(char***)malloc(sizeof(char**)*cus_size);

	struct data_pocket *pocket=bag->first;

	for(int i=0;i<cus_size-1;i++)
	{
		s_response->custom_headers[i]=(char**)malloc(sizeof(char*)*2);

		s_response->custom_headers[i][0]=(char*)memndup(pocket->data,pocket->len);

		pocket=pocket->next;

		if(pocket==NULL)
		{
			s_response->custom_headers[i][1]=NULL;
			break;
		}

		s_response->custom_headers[i][1]=(char*)memndup(pocket->data,pocket->len);

		pocket=pocket->next;
	}

	s_response->custom_headers[cus_size-1]=NULL;

	return s_response;

}

void* send_http_request(int sockfd,struct network_data* request,char* hostname,int flag)
{
	if(request==NULL)
		return NULL;

	if(sockfd<0)
		return NULL;

	if(flag==0)
		flag=SEND_RECEIVE;

	long* status=(long*)malloc(sizeof(long));

	if(flag==JUST_SEND || flag==SEND_RECEIVE)
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

	if(flag==SEND_RECEIVE)
	{
		struct network_data* response=sock_read(sockfd,LONG_MAX);

		return (void*)response;
	}

	return NULL;
}

#ifdef LIBSSL_SANE
void* send_https_request(int sockfd,struct network_data* request,char* hostname,int flag)
{

	if(sockfd<0)
		return NULL;

	SSL* ssl=ssl_open_connection(sockfd,hostname);

	if(ssl==NULL)
		return NULL;

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
#else
void* send_https_request(int sockfd,struct network_data* request,char* hostname,int flag)
{
	https_quit();
	return NULL;
}
#endif

void* follow_redirects(struct http_request* c_s_request,struct network_data* response,long max_redirects,struct socket_info* cli_info,int flag)
{

	if(response==NULL)
		return NULL;

	struct http_response* s_response;
	struct parsed_url* purl;
	struct network_data* request=create_http_request(c_s_request);
	struct http_request* s_request=(struct http_request*)memndup(c_s_request,sizeof(struct http_request));

	long redirect_count=0;

	while(redirect_count<max_redirects)
	{
		s_response=parse_http_response(response);

		if(s_response==NULL)
			return NULL;

		if(s_response->status_code[0]!='3' || s_response->location==NULL)
		{
			break;
		}

		if(args->vverbose_flag && !args->quiet_flag)
		{
			fprintf(stderr,"\nHTTP Redirection Number %ld:\n\n",redirect_count+1);
		}

		purl=parse_url(s_response->location); // Parse the URL

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

		char* hostip=resolve_dns(purl->host); // Resolve DNS

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

		request=create_http_request(s_request); // Create HTTP Request

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
			response=(struct network_data*)send_http_request(sockfd,request,NULL,0);
		}
		else
		{
			response=(struct network_data*)send_https_request(sockfd,request,purl->host,0);
		}

		if(response==NULL)
			return NULL;

		if(args->vverbose_flag && !args->quiet_flag)
		{
			fprintf(stderr,"\n->HTTP Response Message:\n\n");
			fprintf(stderr,"-->");
			sock_write(fileno(stderr),response);
		}

		redirect_count++;
	}


	// Return values as caller requested

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

#ifndef LIBSSL_SANE
void https_quit()
{
	fprintf(stderr,"\nMID: HTTPS URL encountered! MID is not built with the SSL support. Please recompile MID with the SSL support. Exiting...\n\n");
	exit(3);
}
#endif

