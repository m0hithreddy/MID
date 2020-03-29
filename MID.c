/*
 * MID.c
 *
 *  Created on: 08-Mar-2020
 *      Author: Mohith Reddy
 */

#include"MID.h"
#include"MID_arguments.h"
#include"MID_socket.h"
#include"MID_structures.h"
#include"MID_http.h"
#include"MID_ssl_socket.h"
#include"MID_interfaces.h"
#include"url_parser.h"
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<openssl/ssl.h>
#include<openssl/err.h>
#include<unistd.h>
#include<netinet/tcp.h>
#include<pthread.h>
#include "MID_unit.h"

struct mid_args* args;

int main(int argc, char **argv) {

	args=parse_mid_args(argv,argc);

	if( !args->surpass_root_check && getuid()!=0 && geteuid()!=0)
	{
		mid_help("MID: SO_BINDTODEVICE socket-option is used to bind to an interface, which requires root permissions and CAP_NET_RAW capability. If you believe the current UID is having the sufficient permissions then try using --surpass-root-check flag");
	}

	//Parse the URL

	struct parsed_url *purl;
	purl=parse_url(args->url);


	if( purl==NULL || !( !strcmp(purl->scheme,"https") || !strcmp(purl->scheme,"http") ) )
	{
		if(!args->quiet_flag)
			fprintf(stderr,"MID: Not a HTTP or HTTPS URL\nExiting...\n\n");

		exit(1);
	}

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nURL Information:\n\n");

		fprintf(stderr,"->Scheme: %s\n",purl->scheme);
		fprintf(stderr,"->Host: %s\n",purl->host);
		fprintf(stderr,"->Port: %s\n",purl->port);
		fprintf(stderr,"->PATH: %s\n",purl->path);
		fprintf(stderr,"->Query: %s\n",purl->query);
	}

	//DNS Query

	char* hostip=resolve_dns(purl->host);

	if(hostip==NULL)
	{
		if(!args->quiet_flag)
			fprintf(stderr,"MID: Unable to find the IPV4 address of %s\nExiting...\n\n",purl->host);

		exit(1);
	}

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nDNS Resolve:\n\n");
		fprintf(stderr,"->%s IPV4 address: %s\n",purl->host,hostip);
	}

	// Get Network Interfaces Info and Check whether Server is accessible or not

	struct network_interface** net_if=get_net_if_info(args->include_ifs,args->include_ifs_count,args->exclude_ifs,args->exclude_ifs_count);

	if(net_if[0]==NULL)
	{
		if(!args->quiet_flag)
			fprintf(stderr,"MID: No network-interface found for downloading\nExiting...\n\n");

		exit(1);
	}

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nInitially Identified Interfaces:\n\n");

		for(int i=0;net_if[i]!=NULL;i++)
		{
			fprintf(stderr,"->Interface: %s, Address: %s, Netmask: %s, Family: %d\n",net_if[i]->name,net_if[i]->address,net_if[i]->netmask,net_if[i]->family);
		}
	}

	struct sockaddr *servaddr=create_sockaddr_in(hostip,atoi((purl->port!=NULL)? purl->port:(!(strcmp(purl->scheme,"http"))? DEFAULT_HTTP_PORT:DEFAULT_HTTPS_PORT)),DEFAULT_HTTP_SOCKET_FAMILY);

	if(servaddr==NULL)
	{
		if(!args->quiet_flag)
			fprintf(stderr,"MID: Error Checking partial content support\nExiting...\n\n");

		exit(1);
	}


	struct http_request* s_request=(struct http_request*)calloc(1,sizeof(struct http_request));
	struct http_request* gl_s_request=NULL;

	s_request->method="HEAD";

	if(purl->path!=NULL)
	{
		char* tmp_path=(char*)calloc(HTTP_REQUEST_HEADERS_MAX_LEN,sizeof(char));
		strcpy(tmp_path,purl->path);
		if(purl->query!=NULL)
		{
			strcat(tmp_path,"?");
			strcat(tmp_path,purl->query);
		}
		s_request->path=tmp_path;
	}
	else
	{
		s_request->path=NULL;
	}

	s_request->version=MID_DEFAULT_HTTP_VERSION;
	s_request->host=purl->host;
	s_request->port=purl->port;
	s_request->user_agent=MID_USER_AGENT;
	s_request->connection="close";
	s_request->url=args->url;
	s_request->hostip=hostip;
	s_request->custom_headers=args->custom_headers;

	struct network_data* request=create_http_request(s_request);

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nHTTP Request Message:\n\n");
		fprintf(stderr,"->");
		sock_write(fileno(stderr),request);
	}

	struct http_response* gl_s_response=NULL;

	struct data_bag* net_if_bag=create_data_bag();

	struct network_data* net_if_data=(struct network_data*)malloc(sizeof(struct network_data));

	for (int i = 0; net_if[i]!=NULL ; i++)
	{

		if(net_if[i]->family!=AF_INET)
			continue;

		struct socket_info cli_info;

		cli_info.address=net_if[i]->address;
		cli_info.port=0;
		cli_info.family=DEFAULT_HTTP_SOCKET_FAMILY;
		cli_info.type=DEFAULT_HTTP_SOCKET_TYPE;
		cli_info.protocol=DEFAULT_HTTP_SOCKET_PROTOCOL;

		cli_info.sock_opts=(struct socket_opt*)malloc(sizeof(struct socket_opt)*2);
		cli_info.sock_opts[0]=create_socket_opt(SOL_SOCKET,SO_BINDTODEVICE,net_if[i]->name,strlen(net_if[i]->name));
		cli_info.sock_opts[1]=create_socket_opt(IPPROTO_TCP,TCP_SYNCNT,&args->max_tcp_syn_retransmits,sizeof(int));

		cli_info.opts_len=2;


		int sockfd=open_connection(&cli_info,servaddr);

		if(sockfd<0)
		{
			continue;
		}


		struct network_data *response;

		if(!strcmp(purl->scheme,"http"))
			response=send_http_request(sockfd,request,0);
		else
			response=send_https_request(sockfd,request,0);

		close(sockfd);

		if(response==NULL)
		{
			continue;
		}

		if(args->vverbose_flag && !args->quiet_flag)
		{
			fprintf(stderr,"\nHTTP Response Message:\n\n");
			fprintf(stderr,"->");
			sock_write(fileno(stderr),response);
		}

		void* s_rqst_s_resp=follow_redirects(s_request, response, args->max_redirects,&cli_info,RETURN_S_REQUEST_S_RESPONSE);

		if(s_rqst_s_resp==NULL)
		{
			continue;
		}

		struct http_request* tmp_s_request=(struct http_request*)s_rqst_s_resp;

		struct http_response* tmp_s_response=(struct http_response*)(s_rqst_s_resp+sizeof(struct http_request));

		if(gl_s_response==NULL)
		{
			gl_s_response=tmp_s_response;
			gl_s_request=tmp_s_request;
		}

		if(strcmp(tmp_s_response->status_code,"200"))
			continue;
		else
		{
			gl_s_request=tmp_s_request;
			gl_s_response=tmp_s_response;
		}

		net_if_data->data=(char*)net_if[i];
		net_if_data->len=sizeof(struct network_interface);

		place_data(net_if_bag,net_if_data);
	}

	if(gl_s_response==NULL)
	{
		if(!args->quiet_flag)
			fprintf(stderr,"MID: Error reading server response\nExiting...\n\n");

		exit(1);
	}
	else
	{
		if(gl_s_response->status_code[0]=='4')
		{
			if(!args->quiet_flag)
				fprintf(stderr,"MID: Client Side Error, Status-Code: %s , Status: %s\nExiting...\n\n",gl_s_response->status_code,gl_s_response->status);

			exit(1);
		}
		else if(gl_s_response->status_code[0]=='5')
		{
			if(!args->quiet_flag)
				fprintf(stderr,"MID: Server Side Error, Status-Code: %s , Status: %s\nExiting...\n\n",gl_s_response->status_code,gl_s_response->status);

			exit(1);
		}
		else if(gl_s_response->status_code[0]!='2')
		{
			if(!args->quiet_flag)
				fprintf(stderr,"MID: Unknown error reported by server, Status-Code: %s , Status: %s\nExiting...\n\n",gl_s_response->status_code,gl_s_response->status);

			exit(1);
		}
	}

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nFew HTTP Response Message Fields:\n\n");
		fprintf(stderr,"->Response Version: %s\n",gl_s_response==NULL? "No response": gl_s_response->version);
		fprintf(stderr,"->Response Status-Code: %s\n",gl_s_response==NULL? "No response":gl_s_response->status_code);
		fprintf(stderr,"->Response Status: %s\n",gl_s_response==NULL? "No response":gl_s_response->status);
		fprintf(stderr,"->Response Date: %s\n",gl_s_response==NULL? "No response":gl_s_response->date);
		fprintf(stderr,"->Response Server: %s\n",gl_s_response==NULL? "No response":gl_s_response->server);
		fprintf(stderr,"->Response Accept-Ranges: %s\n",gl_s_response==NULL? "No response":gl_s_response->accept_ranges);
		fprintf(stderr,"->Response Connection: %s\n",gl_s_response==NULL? "No response":gl_s_response->connection);
		fprintf(stderr,"->Response Location: %s\n",gl_s_response==NULL? "No response":gl_s_response->location);
	}

	net_if_data=flatten_data_bag(net_if_bag);

	if(net_if_data==NULL)
	{
		if(!args->quiet_flag)
			fprintf(stderr,"MID: No suitable network-interface found for downloading\nExiting...\n\n");

		exit(1);
	}

	struct network_interface* ok_net_if=(struct network_interface*)net_if_data->data;
	long ok_net_if_len=net_if_bag->n_pockets;

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nActual Interfaces Used For Downloading:\n\n");
		for(int i=0; i<ok_net_if_len ;i++)
		{
			if(ok_net_if==NULL)
				break;
			fprintf(stderr,"->OK Interface: %s Address: %s\n",ok_net_if[i].name,ok_net_if[i].address);
		}
	}


	if(ok_net_if_len==0)
	{
		if(!args->quiet_flag)
			fprintf(stderr,"MID: No suitable network-interface found for downloading\nExiting...\n\n");

		exit(1);
	}

	int pc_flag=1;

	if(gl_s_response->accept_ranges==NULL || strcmp(gl_s_response->accept_ranges,"bytes") || gl_s_response->content_length==NULL)
		pc_flag=0;

	//Base client socket_info structure

	struct socket_info* base_socket_info=(struct socket_info*)malloc(sizeof(struct socket_info));

	base_socket_info->address=ok_net_if[0].address;
	base_socket_info->port=0;
	base_socket_info->family=DEFAULT_HTTP_SOCKET_FAMILY;
	base_socket_info->type=DEFAULT_HTTP_SOCKET_TYPE;
	base_socket_info->protocol=DEFAULT_HTTP_SOCKET_PROTOCOL;

	base_socket_info->sock_opts=(struct socket_opt*)malloc(sizeof(struct socket_opt)*2);
	base_socket_info->sock_opts[0]=create_socket_opt(SOL_SOCKET,SO_BINDTODEVICE,ok_net_if[0].name,strlen(ok_net_if[0].name));
	base_socket_info->sock_opts[1]=create_socket_opt(IPPROTO_TCP,TCP_SYNCNT,&args->max_tcp_syn_retransmits,sizeof(int));

	base_socket_info->opts_len=2;


	//Base unit_info structure

	struct unit_info* base_unit_info=(struct unit_info*)calloc(1,sizeof(struct unit_info));

	if(args->output_file!=NULL)
		base_unit_info->file=determine_filename(args->output_file);
	else
		base_unit_info->file=determine_filename(purl->path);

	base_unit_info->if_name=ok_net_if[0].name;
	base_unit_info->if_id=0;
	base_unit_info->current_size=0;
	base_unit_info->total_size=0;
	base_unit_info->report_size=(long*)calloc(ok_net_if_len,sizeof(long));
	base_unit_info->report_len=ok_net_if_len;
	base_unit_info->max_unit_retries=args->max_unit_retries;
	base_unit_info->resume=1;
	base_unit_info->quit=0;
	base_unit_info->pc_flag=0;
	base_unit_info->err_flag=0;
	base_unit_info->self_repair=0;
	base_unit_info->status_code=0;
	base_unit_info->transfer_encoding=0;
	base_unit_info->unit_retry_sleep_time=args->unit_retry_sleep_time;
	base_unit_info->healing_time=0;
	base_unit_info->start_time=0;

	base_unit_info->cli_info=base_socket_info;
	base_unit_info->unit_ranges=NULL;

	purl=parse_url(gl_s_request->url);
	base_unit_info->servaddr=create_sockaddr_in(gl_s_request->hostip,atoi((purl->port!=NULL)? purl->port:(!(strcmp(purl->scheme,"http"))? DEFAULT_HTTP_PORT:DEFAULT_HTTPS_PORT)),DEFAULT_HTTP_SOCKET_FAMILY);

	base_unit_info->s_request=(struct http_request*)malloc(sizeof(struct http_request));
	memcpy(base_unit_info->s_request,gl_s_request,sizeof(struct http_request));

	base_unit_info->range=(struct http_range*)malloc(sizeof(struct http_range));
	base_unit_info->range->start=0;
	base_unit_info->range->end=-1;

	base_unit_info->unit_ranges=create_data_bag();

	//Creating the file

	if(!args->quiet_flag)
	{
		printf("\nOpening File: %s\n",base_unit_info->file);
	}

	fclose(fopen(base_unit_info->file,"w"));

	// Creating thread for Progress Display;

	struct show_progress_info* s_progress_info=(struct show_progress_info*)malloc(sizeof(struct show_progress_info));

	if(!args->quiet_flag)
	{
		s_progress_info->report=NULL;
		s_progress_info->report_len=ok_net_if_len;
		s_progress_info->progress=NULL;
		s_progress_info->content_length=0;
		s_progress_info->sleep_time=args->progress_update_time;
		s_progress_info->quit=0;
		s_progress_info->detailed_progress=args->detailed_progress;

		pthread_mutex_init(&s_progress_info->lock, NULL);
		pthread_create(&s_progress_info->tid,NULL,show_progress,(void*)s_progress_info);
	}

	// Initiating download

	struct data_bag* units_bag=create_data_bag();
	struct network_data* n_unit=(struct network_data*)malloc(sizeof(struct network_data));
	struct unit_info** units=NULL;
	long units_len=0;
	struct interface_report* prev=NULL;
	struct interface_report* current=NULL;
	struct units_progress* progress=NULL;
	struct unit_info* idle=NULL;
	struct unit_info* err=NULL;
	time_t start_time=time(NULL);
	long downloaded_length=0;
	long content_length=0;

	if(pc_flag==0) // If range requests are not supported
	{
		pthread_mutex_init(&base_unit_info->lock,NULL);
		pthread_create(&base_unit_info->unit_id,NULL,unit,base_unit_info);

		n_unit->data=(void*)&base_unit_info;
		n_unit->len=sizeof(struct base_unit_info*);

		place_data(units_bag,n_unit);

		units=(struct unit_info**)flatten_data_bag(units_bag)->data;
		units_len=units_bag->n_pockets;

		while(1)
		{
			sleep(1);

			current=get_interface_report(units,units_len,ok_net_if,ok_net_if_len,prev);
			prev=current;

			progress=actual_progress(units, units_len);

			if(!args->quiet_flag)
			{
				pthread_mutex_lock(&s_progress_info->lock);

				s_progress_info->report=current;
				s_progress_info->progress=progress;

				pthread_mutex_unlock(&s_progress_info->lock);
			}

			idle=idle_unit(units,units_len);

			if(idle!=NULL)
			{
				if(idle->err_flag==1)
					err=idle;
				break;
			}
		}

		progress=actual_progress(units, units_len);

		downloaded_length=progress->content_length;

	}

	else // If range requests are supported
	{

		content_length=atol(gl_s_response->content_length);
		s_progress_info->content_length=content_length;

		//Initiating the first partial request

		struct unit_info* unit_info=(struct unit_info*)malloc(sizeof(struct unit_info));
		memcpy(unit_info,base_unit_info,sizeof(struct unit_info));

		unit_info->pc_flag=1;

		unit_info->cli_info=(struct socket_info*)malloc(sizeof(struct socket_info));
		memcpy(unit_info->cli_info,base_socket_info,sizeof(struct socket_info));

		unit_info->cli_info->sock_opts=(struct socket_opt*)malloc(sizeof(struct socket_opt)*2);
		memcpy(unit_info->cli_info->sock_opts,base_socket_info->sock_opts,sizeof(struct socket_opt)*2);

		unit_info->s_request=(struct http_request*)malloc(sizeof(struct http_request));
		memcpy(unit_info->s_request,gl_s_request,sizeof(struct http_request));

		unit_info->range=(struct http_range*)malloc(sizeof(struct http_range));
		unit_info->range->start=0;
		unit_info->range->end=content_length-1;

		unit_info->unit_ranges=create_data_bag();

		pthread_mutex_init(&unit_info->lock,NULL);
		pthread_create(&unit_info->unit_id,NULL,unit,unit_info);

		//

		n_unit->data=(void*)&unit_info;
		n_unit->len=sizeof(struct unit_info*);

		place_data(units_bag,n_unit);

		// Scheduling download across different interfaces

		long if_id=0;
		struct unit_info* largest=NULL;
		struct unit_info* update=NULL;

		long sleep_time=MIN_SCHED_SLEEP_TIME;
		long max_parallel_downloads=args->max_parallel_downloads;


		while(1)
		{

			units=(struct unit_info**)flatten_data_bag(units_bag)->data;
			units_len=units_bag->n_pockets;

			current=get_interface_report(units,units_len,ok_net_if,ok_net_if_len,prev);

			progress=actual_progress(units, units_len);

			if(!args->quiet_flag)
			{
				pthread_mutex_lock(&s_progress_info->lock);

				s_progress_info->report=current;
				s_progress_info->progress=progress;

				pthread_mutex_unlock(&s_progress_info->lock);

			}

			if(progress!=NULL)
			{
				downloaded_length=progress->content_length;
			}

			if(downloaded_length==content_length) // Download complete
				break;

			if(!args->quiet_flag)
			{
				pthread_mutex_lock(&s_progress_info->lock);

				s_progress_info->report=current;
				s_progress_info->progress=progress;

				pthread_mutex_unlock(&s_progress_info->lock);

			}

			if_id=scheduler(current,prev,ok_net_if_len,if_id);
			prev=current;

			if(if_id<0)
			{
				sleep(sleep_time);

				sleep_time++;
				if(sleep_time>MAX_SCHED_SLEEP_TIME)
					sleep_time=MIN_SCHED_SLEEP_TIME;

				continue;
			}
			else
			{
				sleep(1);
				sleep_time=MIN_SCHED_SLEEP_TIME;
			}

			idle=idle_unit(units,units_len);

			if(idle==NULL && update==NULL)
			{
				if(units_bag->n_pockets>=max_parallel_downloads)
					continue;

				update=(struct unit_info*)malloc(sizeof(struct unit_info));
				memcpy(update,base_unit_info,sizeof(struct unit_info));

				update->pc_flag=1;
				update->report_size=(long*)calloc(ok_net_if_len,sizeof(long));

				update->cli_info=(struct socket_info*)malloc(sizeof(struct socket_info));
				memcpy(update->cli_info,base_socket_info,sizeof(struct socket_info));

				update->cli_info->sock_opts=(struct socket_opt*)malloc(sizeof(struct socket_opt)*2);
				memcpy(update->cli_info->sock_opts,base_socket_info->sock_opts,sizeof(struct socket_opt)*2);

				update->s_request=(struct http_request*)malloc(sizeof(struct http_request));
				memcpy(update->s_request,gl_s_request,sizeof(struct http_request));

				update->range=(struct http_range*)malloc(sizeof(struct http_range));

				update->unit_ranges=create_data_bag();

			}
			else
			{
				if(idle->err_flag==1)
				{
					err=idle;
					break;
				}

				update=idle;
			}

			update->if_name=ok_net_if[if_id].name;
			update->if_id=if_id;

			update->cli_info->address=ok_net_if[if_id].address;
			update->cli_info->sock_opts[0]=create_socket_opt(SOL_SOCKET,SO_BINDTODEVICE,ok_net_if[if_id].name,strlen(ok_net_if[if_id].name));

			largest=largest_unit(units,units_len);

			if(largest==NULL)
			{
				continue;
			}

			pthread_mutex_lock(&largest->lock);

			long rem_chunk=largest->range->end-largest->range->start+1-largest->current_size;

			update->range->end=largest->range->end;
			largest->range->end=largest->range->start+largest->current_size+rem_chunk/2;
			update->range->start=largest->range->end+1;

			pthread_mutex_unlock(&largest->lock);

			if(idle==NULL)
			{
				pthread_mutex_init(&update->lock,NULL);
				pthread_create(&update->unit_id,NULL,unit,update);

				n_unit->data=(void*)&update;
				n_unit->len=sizeof(struct unit_info*);

				place_data(units_bag,n_unit);

				update=NULL;
			}
			else
			{
				update->resume=1;
				update=NULL;
			}
		}
	}

	time_t end_time=time(NULL);

	suspend_units((struct unit_info**)(flatten_data_bag(units_bag)->data),units_bag->n_pockets);

	if(!args->quiet_flag)
	{
		pthread_mutex_lock(&s_progress_info->lock);

		s_progress_info->quit=1;

		pthread_mutex_unlock(&s_progress_info->lock);

		pthread_join(s_progress_info->tid,NULL);
		pthread_mutex_destroy(&s_progress_info->lock);

	}

	printf("\n---------\n");
	printf("|SUMMARY|\n");
	printf("---------\n\n");
	printf("Time Spent = %s     Total Downloaded = %s     Average Speed = %s\n\n",convert_time(end_time-start_time),convert_data(downloaded_length,0), end_time-start_time !=0 ? convert_speed(downloaded_length/(end_time-start_time)) : "undef");

	if(err!=NULL)
	{
		fprintf(stderr,"MID: Error downloading chunk\nExiting...\n\n");
	}


	return 0;


}
