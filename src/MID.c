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
#include"MID_unit.h"
#include"MID_err.h"
#include"url_parser.h"
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/tcp.h>
#include<pthread.h>
#include<sys/file.h>
#include<stddef.h>
#include<signal.h>
#include<sys/stat.h>
#include<sys/types.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

struct mid_args* args;

FILE* o_fp=NULL;
FILE* u_fp=NULL;

struct http_request* gl_s_request=NULL;
struct http_response* gl_s_response=NULL;

struct unit_info* base_unit_info=NULL;

int handler_registered=0;
struct signal_handler_info* s_hd_info=NULL;

struct data_bag* units_bag=NULL;

long content_length=0;
long downloaded_length=0;

int main(int argc, char **argv)
{

	args=parse_mid_args(argv,argc);

	if( !args->surpass_root_check && getuid()!=0 && geteuid()!=0)
	{
		mid_help("MID: SO_BINDTODEVICE socket-option is used to bind to an interface, which requires root permissions and CAP_NET_RAW capability. If you believe the current UID is having sufficient permissions then try using {--surpass-root-check flag | -s}.");
	}

	//Parse the URL

	struct parsed_url *purl;
	purl=parse_url(args->url);


	if( purl==NULL || !( !strcmp(purl->scheme,"https") || !strcmp(purl->scheme,"http") ) )
		mid_flag_exit(1,"MID: Not a HTTP or HTTPS URL. Exiting...\n\n");

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
		mid_flag_exit(1,"MID: Unable to find the IPV4 address of %s. Exiting...\n\n",purl->host);

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nDNS Resolve:\n\n");
		fprintf(stderr,"->%s IPV4 address: %s\n",purl->host,hostip);
	}

	// Get Network Interfaces Info and Check whether Server is accessible or not

	struct network_interface** net_if=get_net_if_info(args->include_ifs,args->include_ifs_count,args->exclude_ifs,args->exclude_ifs_count);

	if(net_if[0]==NULL)
		mid_flag_exit(1,"MID: No network-interface found for downloading. Exiting...\n\n");

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
		mid_flag_exit(1,"MID: Error Checking partial content support. Exiting...\n\n");

	struct http_request* s_request=(struct http_request*)calloc(1,sizeof(struct http_request));

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
#ifdef LIBZ_SANE
	s_request->accept_encoding="gzip";
#endif
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

	struct data_bag* net_if_bag=create_data_bag();

	struct network_data* net_if_data=(struct network_data*)malloc(sizeof(struct network_data));

	for (int i = 0; net_if[i]!=NULL ; i++) // for each network interface check whether server is accessible
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
			response=send_http_request(sockfd,request,NULL,0);
		else
			response=send_https_request(sockfd,request,purl->host,0);

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
		mid_flag_exit(1,"MID: Error reading server response. Exiting...\n\n");
	else
	{
		if(gl_s_response->status_code[0]=='4')
			mid_flag_exit(1,"MID: Client Side Error, Status-Code: %s , Status: %s. Exiting...\n\n",gl_s_response->status_code,gl_s_response->status);
		else if(gl_s_response->status_code[0]=='5')
			mid_flag_exit(1,"MID: Server Side Error, Status-Code: %s , Status: %s. Exiting...\n\n",gl_s_response->status_code,gl_s_response->status);
		else if(gl_s_response->status_code[0]!='2')
			mid_flag_exit(1,"MID: Unknown error reported by server, Status-Code: %s , Status: %s. Exiting...\n\n",gl_s_response->status_code,gl_s_response->status);
	}

	// Some debug information

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

	// Actual Interfaces

	net_if_data=flatten_data_bag(net_if_bag);

	if(net_if_data==NULL)
		mid_flag_exit(1,"MID: No suitable network-interface found for downloading. Exiting...\n\n");

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
		mid_flag_exit(1,"MID: No suitable network-interface found for downloading. Exiting...\n\n");

	// Check for Range request support

	int pc_flag=1;

	if(gl_s_response->accept_ranges==NULL || strcmp(gl_s_response->accept_ranges,"bytes") || gl_s_response->content_length==NULL)
		pc_flag=0;

	//Final values for Host Port and URL scheme after redirects

	struct parsed_url* fin_purl=parse_url(gl_s_request->url);

	char* fin_host=strdup(fin_purl->host);
	char* fin_scheme=strdup(fin_purl->scheme);
	long fin_port=atoi((fin_purl->port!=NULL)? fin_purl->port:(!(strcmp(fin_scheme,"http"))? DEFAULT_HTTP_PORT:DEFAULT_HTTPS_PORT));

	char** fin_hostips=resolve_dns_mirros(fin_host,&(args->max_mirrors));

	if(fin_hostips==NULL)
		mid_flag_exit(1,"MID: No mirrors found for %s. Exiting...\n\n",fin_host);

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

	base_unit_info=(struct unit_info*)calloc(1,sizeof(struct unit_info));

	if(args->output_file!=NULL)
		base_unit_info->file=determine_filename(args->output_file,&o_fp);
	else
		base_unit_info->file=path_to_filename(purl->path,&o_fp);

	if(gl_s_response->content_encoding!=NULL)
	{
		if(args->up_file!=NULL)
		{
			base_unit_info->up_file=determine_filename(args->up_file,&u_fp);
		}
		else
		{
			long tmp_len=strlen(base_unit_info->file);
			char tmp_up_file[tmp_len+4];

			memcpy(tmp_up_file,base_unit_info->file,tmp_len);

			tmp_up_file[tmp_len]='.';
			tmp_up_file[tmp_len+1]='u';
			tmp_up_file[tmp_len+2]='p';
			tmp_up_file[tmp_len+3]='\0';

			base_unit_info->up_file=determine_filename(tmp_up_file,&u_fp);
		}
	}

	base_unit_info->if_name=ok_net_if[0].name;
	base_unit_info->scheme=fin_scheme;
	base_unit_info->host=fin_host;
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

	base_unit_info->servaddr=create_sockaddr_in(*fin_hostips,fin_port,DEFAULT_HTTP_SOCKET_FAMILY);

	base_unit_info->s_request=(struct http_request*)malloc(sizeof(struct http_request));
	memcpy(base_unit_info->s_request,gl_s_request,sizeof(struct http_request));

	base_unit_info->range=(struct http_range*)malloc(sizeof(struct http_range));
	base_unit_info->range->start=0;
	base_unit_info->range->end=-1;

	base_unit_info->unit_ranges=create_data_bag();

	// Printing file names

	if(!args->quiet_flag && gl_s_response->content_encoding == NULL)
	{
		printf("\nOpening File: %s\n",base_unit_info->file);
	}
	else if(!args->quiet_flag)
	{
		printf("\nOpening Unprocessed File: %s\n",base_unit_info->up_file);
	}

	// Registering thread to handle signals

	handler_registered=0;
s_hd_info=(struct signal_handler_info*)calloc(1,sizeof(struct signal_handler_info));

	sigset_t oldmask;

	s_hd_info->quit=0;

	sigemptyset(&s_hd_info->mask);
	sigaddset(&s_hd_info->mask, SIGINT);
	sigaddset(&s_hd_info->mask, SIGQUIT);

	if(pthread_sigmask(SIG_BLOCK, &s_hd_info->mask, &oldmask) != 0)  // Block SIGINT and SIGQUIT
		mid_flag_exit(1,"MID: Error initiating the signal handler. Exiting...\n\n");

	pthread_mutex_init(&s_hd_info->lock,NULL);
	pthread_create(&s_hd_info->tid, NULL,signal_handler,(void*)s_hd_info);

	handler_registered=1;

	// Initiating download

	units_bag=create_data_bag();
	struct network_data* n_unit=(struct network_data*)malloc(sizeof(struct network_data));
	struct unit_info** units=NULL;
	long units_len=0;
	struct interface_report* prev=NULL;
	struct interface_report* current=NULL;
	struct units_progress* progress=NULL;
	struct unit_info* idle=NULL;
	struct unit_info* err=NULL;
	time_t start_time;
	downloaded_length=0;
	content_length=0;

	// Creating thread for Progress Display;

	struct show_progress_info* s_progress_info;

	if(!args->quiet_flag)
	{
		s_progress_info=(struct show_progress_info*)malloc(sizeof(struct show_progress_info));

		s_progress_info->units_bag=units_bag;
		s_progress_info->ifs=ok_net_if;
		s_progress_info->ifs_len=ok_net_if_len;
		s_progress_info->content_length=content_length;
		s_progress_info->sleep_time=args->progress_update_time;
		s_progress_info->quit=0;
		s_progress_info->detailed_progress=args->detailed_progress;

		pthread_mutex_init(&s_progress_info->lock, NULL);
		pthread_create(&s_progress_info->tid,NULL,show_progress,(void*)s_progress_info);
	}

	start_time=time(NULL);

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
			pthread_mutex_lock(&s_hd_info->lock);

			if(s_hd_info->quit)
			{
				pthread_mutex_unlock(&s_hd_info->lock);
				break;
			}

			pthread_mutex_unlock(&s_hd_info->lock);

			sleep(SCHEDULER_DEFAULT_SLEEP_TIME);

			idle=idle_unit(units,units_len);

			if(idle!=NULL)
			{
				if(idle->err_flag==1)
					err=idle;
				break;
			}
		}

	}

	else // If range requests are supported
	{

		content_length=atol(gl_s_response->content_length);

		if(!args->quiet_flag)
			s_progress_info->content_length=content_length;

		//Initiating the first range request

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

		// Push first unit info to units_bag

		n_unit->data=(void*)&unit_info;
		n_unit->len=sizeof(struct unit_info*);

		place_data(units_bag,n_unit);

		// Scheduling download across different interfaces

		long if_id=0;
		long hostip_id=0;
		struct unit_info* largest=NULL;
		struct unit_info* update=NULL;
		struct unit_info* new=NULL;

		long sleep_time=MIN_SCHED_SLEEP_TIME;
		long max_parallel_downloads=args->max_parallel_downloads;

		struct scheduler_info* sch_info=(struct scheduler_info*)calloc(1,sizeof(struct scheduler_info));

		sch_info->current=NULL;
		sch_info->ifs=ok_net_if;
		sch_info->ifs_len=ok_net_if_len;
		sch_info->max_speed=(long*)calloc(ok_net_if_len,sizeof(long));
		sch_info->max_connections=(long*)calloc(ok_net_if_len,sizeof(long));
		sch_info->max_parallel_downloads=max_parallel_downloads;
		sch_info->sch_id=0;
		sch_info->sleep_time=SCHEDULER_DEFAULT_SLEEP_TIME;
		sch_info->probing_done=0;

		while(1)
		{
			pthread_mutex_lock(&s_hd_info->lock);

			if(s_hd_info->quit)
			{
				pthread_mutex_unlock(&s_hd_info->lock);
				break;
			}

			pthread_mutex_unlock(&s_hd_info->lock);

			units=(struct unit_info**)flatten_data_bag(units_bag)->data;
			units_len=units_bag->n_pockets;

			sleep(sch_info->sleep_time);


			current=get_interface_report(units,units_len,ok_net_if,ok_net_if_len,prev);

			progress=get_units_progress(units, units_len);

			downloaded_length=progress->content_length;

			if(downloaded_length==content_length) // Download complete
				break;

			sch_info->current=current;

			scheduler(sch_info); // Scheduler decision

			if_id=sch_info->sch_id;

			prev=current;

			if(if_id<0)
			{
				continue;
			}

			idle=idle_unit(units,units_len);

			if(idle==NULL && new==NULL)
			{
				// Create a new unit

				if(units_bag->n_pockets>=max_parallel_downloads)
					continue;

				new=(struct unit_info*)malloc(sizeof(struct unit_info));
				memcpy(new,base_unit_info,sizeof(struct unit_info));

				new->pc_flag=1;
				new->report_size=(long*)calloc(ok_net_if_len,sizeof(long));

				new->cli_info=(struct socket_info*)malloc(sizeof(struct socket_info));
				memcpy(new->cli_info,base_socket_info,sizeof(struct socket_info));

				new->cli_info->sock_opts=(struct socket_opt*)malloc(sizeof(struct socket_opt)*2);
				memcpy(new->cli_info->sock_opts,base_socket_info->sock_opts,sizeof(struct socket_opt)*2);

				new->servaddr=create_sockaddr_in(fin_hostips[hostip_id], fin_port, DEFAULT_HTTP_SOCKET_FAMILY);

				new->s_request=(struct http_request*)malloc(sizeof(struct http_request));
				memcpy(new->s_request,gl_s_request,sizeof(struct http_request));

				new->range=(struct http_range*)malloc(sizeof(struct http_range));

				new->unit_ranges=create_data_bag();

				update=new;

			}
			else if(idle==NULL && new!=NULL)
			{
				// Use already created but not used unit

				update=new;
			}
			else
			{
				if(idle->err_flag==1) // Check for errors, if found terminate download
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

			if(largest==NULL) // If all busy in error recovery
			{
				continue;
			}

			pthread_mutex_lock(&largest->lock);

			// Break the unit into two

			long rem_chunk=largest->range->end-largest->range->start+1-largest->current_size;

			update->range->end=largest->range->end;

			if(rem_chunk%2==1)
				largest->range->end=largest->range->start+largest->current_size+rem_chunk/2;
			else
				largest->range->end=largest->range->start+largest->current_size+(rem_chunk/2)-1;

			update->range->start=largest->range->end+1;

			pthread_mutex_unlock(&largest->lock);

			// Assign a mirror to download

			hostip_id=(hostip_id+1)%args->max_mirrors;
			update->servaddr=create_sockaddr_in(fin_hostips[hostip_id], fin_port, DEFAULT_HTTP_SOCKET_FAMILY);

			// Resume or initiate the unit

			if(idle==NULL)
			{
				pthread_mutex_init(&update->lock,NULL);
				pthread_create(&update->unit_id,NULL,unit,update);

				n_unit->data=(void*)&update;
				n_unit->len=sizeof(struct unit_info*);

				if(!args->quiet_flag)
					pthread_mutex_lock(&s_progress_info->lock);

				place_data(units_bag,n_unit);

				if(!args->quiet_flag)
					pthread_mutex_unlock(&s_progress_info->lock);

				new=NULL;
			}
			else
			{
				update->resume=1;
			}
		}
	}

	// End of the download

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


	units=(struct unit_info**)flatten_data_bag(units_bag)->data;
	units_len=units_bag->n_pockets;

	progress=get_units_progress(units, units_len);

	downloaded_length=progress->content_length;

	if(!args->quiet_flag)
	{
		printf("\n|SUMMARY|\n\n");
		printf("Time Spent = %s     Total Downloaded = %s     Average Speed = %s\n\n",convert_time(end_time-start_time),convert_data(downloaded_length,0), end_time-start_time !=0 ? convert_speed(downloaded_length/(end_time-start_time)) : "undef");
	}


	if(err!=NULL)
		mid_flag_exit(2,"MID: Error downloading chunk. Exiting...\n\n");

	pthread_mutex_lock(&s_hd_info->lock);
	if(s_hd_info->quit)
	{
		pthread_mutex_unlock(&s_hd_info->lock);
		save_mid_state();

		mid_exit(1);
	}
	pthread_mutex_unlock(&s_hd_info->lock);

	// Handle Content-Encoding

	if(gl_s_response->content_encoding==NULL)
		mid_exit(0);

	if(!args->quiet_flag)
		printf("Decoding the file: %s\n",base_unit_info->up_file);

	struct encoding_info* en_info=determine_encodings(gl_s_response->content_encoding); // Determine the encodings

	if(en_info==NULL)
		mid_flag_exit(3,"MID: Content-Encoding unknown, not removing the unprocessed file %s. Exiting...\n\n",base_unit_info->up_file);

	if(!args->quiet_flag)
		printf("\nOpening File: %s\n\n",base_unit_info->file);

	char uf_data[MAX_TRANSACTION_SIZE];
	int status;
	int en_status=EN_OK;

	en_info->in=uf_data;

	while(1)
	{
		pthread_mutex_lock(&s_hd_info->lock);
		if(s_hd_info->quit)
		{
			pthread_mutex_unlock(&s_hd_info->lock);
			save_mid_state();

			mid_exit(2);
		}
		pthread_mutex_unlock(&s_hd_info->lock);

		status=read(fileno(u_fp),uf_data,MAX_TRANSACTION_SIZE); // Read the unprocessed file.

		if(status<0)
			mid_flag_exit(2,"MID: Error reading the file %s, not removing the unprocessed file %s. Exiting...\n\n",base_unit_info->up_file,base_unit_info->up_file);
		else if(status==0)
		{
			remove(base_unit_info->up_file);

			mid_exit(0);
		}

		en_info->in_max=status;
		en_info->in_len=0;

		while(en_info->in_len!=en_info->in_max) // Decode and write the data to the file.
		{
			if(en_status==EN_END)
				mid_flag_exit(4,"MID: Misc data found at the end of the file %s, not removing the unprocessed file %s. Exiting...\n\n",base_unit_info->up_file,base_unit_info->up_file);

			en_status=handle_encodings(en_info);

			if(en_status!=EN_OK && en_status!=EN_END)
				mid_flag_exit(2,"MID: Error encountered when decoding the file %s, not removing the unprocessed file %s. Exiting...\n",base_unit_info->up_file,base_unit_info->up_file);

			if(write(fileno(o_fp),en_info->out,en_info->out_len)!=en_info->out_len)
				mid_flag_exit(2,"\nMID: Error writing to the file %s, not removing the unprocessed file %s. Exiting...\n\n",base_unit_info->file,base_unit_info->up_file);

		}
	}

	mid_exit(0);
}

void close_files()
{
	if(o_fp!=NULL)
	{
		flock(fileno(o_fp),LOCK_UN);
		fclose(o_fp);
		o_fp=NULL;
	}

	if(u_fp!=NULL)
	{
		flock(fileno(u_fp),LOCK_UN);
		fclose(u_fp);
		u_fp=NULL;
	}
}

void* signal_handler(void* v_s_hd_info)
{

	struct signal_handler_info* s_hd_info=(struct signal_handler_info*)v_s_hd_info;

	int err,signo;

	err=sigwait(&s_hd_info->mask, &signo);

	if(err != 0)
		mid_flag_exit(1,"MID: Error initiating the signal handler. Exiting...\n\n");

	pthread_mutex_lock(&s_hd_info->lock);

	s_hd_info->quit=1;

	pthread_mutex_unlock(&s_hd_info->lock);

	return NULL;
}

void deregister_handler()
{
	if(handler_registered && s_hd_info!=NULL)
	{
		pthread_kill(s_hd_info->tid,SIGINT);
		pthread_join(s_hd_info->tid,NULL);
		pthread_mutex_destroy(&s_hd_info->lock);
	}
}

char* get_ms_filename()
{
	char* ms_file;

	if(args->ms_file==NULL)
	{
#ifdef LIBSSL_SANE
		unsigned char md_result[MD5_DIGEST_LENGTH];

		MD5_CTX md_context;

		MD5_Init(&md_context);

		MD5_Update(&md_context, args->url, strlen(args->url));
		MD5_Final(md_result, &md_context);

		ms_file=(char*)malloc(sizeof(char)*(4+(MD5_DIGEST_LENGTH*8)/4));

		for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
			sprintf(ms_file+2*i,"%02x", md_result[i]);

		ms_file[MD5_DIGEST_LENGTH]='.';
		ms_file[MD5_DIGEST_LENGTH+1]='m';
		ms_file[MD5_DIGEST_LENGTH+2]='s';
		ms_file[MD5_DIGEST_LENGTH+3]='\0';

#else
		ms_file=(char*)malloc(sizeof(char)*(4+strlen(gl_s_request->url)));
		memcpy(ms_file,url_to_filename(gl_s_request->url),strlen(gl_s_request->url));

		ms_file[strlen(gl_s_request->url)]='.';
		ms_file[strlen(gl_s_request->url)+1]='m';
		ms_file[strlen(gl_s_request->url)+2]='s';
		ms_file[strlen(gl_s_request->url)+3]='\0';
#endif
	}
	else
	{
		ms_file=strdup(args->ms_file);
	}

	return ms_file;
}

void save_mid_state()
{
	if(gl_s_request==NULL || gl_s_response == NULL || gl_s_response->status_code[0]!='2') // Enough progress isn't made or not an expected status code to save the state.
		return;

	if(gl_s_response->accept_ranges==NULL || strcmp(gl_s_response->accept_ranges,"bytes") || gl_s_response->content_length==NULL) // Non resumable download
		return;

	if(base_unit_info == NULL || base_unit_info->file == NULL || (gl_s_response->content_encoding!=NULL && base_unit_info->up_file==NULL)) // Even files are not decided yet!
		return;

	if(units_bag==NULL) // No download is initiated yet!
		return;

	if(content_length==downloaded_length && gl_s_response->content_encoding==NULL) // Already downloaded and decoding is not required.
		return;

	// If pased the above then eligible for saving state

	char* ms_file=get_ms_filename();

	FILE* ms_fp=fopen(ms_file,"a+");

	if(ms_fp==NULL) // May be file name too long... Returning to prevent any state files clashes
	{
		if(!args->quiet_flag)
			fprintf(stderr,"\nMID: Unable to open MID state file %s, not saving the state information\n\n",ms_file);

		return;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0)
	{
		if(!args->quiet_flag)
			fprintf(stderr,"\nMID: Unable to acquire lock on MID state file %s, not saving the state information\n\n",ms_file);

		fclose(ms_fp);

		return;
	}

#ifdef LIBSSL_SANE
	if(args->detailed_save)
		dump_detailed_mid_state(ms_fp);
	else
		dump_mid_state(ms_fp);
#else
	dump_mid_state(ms_fp);
#endif

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	return;
}

void dump_int(struct data_bag* bag,int num)
{
	struct network_data n_data;

	n_data.data=(void*)&num;
	n_data.len=sizeof(int);
	place_data(bag,&n_data);
}

void dump_long(struct data_bag* bag,long num)
{
	struct network_data n_data;

	n_data.data=(void*)&num;
	n_data.len=sizeof(long);
	place_data(bag,&n_data);
}

void dump_string(struct data_bag* bag,char* string)
{
	struct network_data n_data;
	long len=strlen(string)+1;

	dump_long(bag,len);

	n_data.data=(void*)string;
	n_data.len=len;
	place_data(bag,&n_data);
}

char* extract_string(FILE* ms_fp)
{
	long len;

	if(read(fileno(ms_fp),&len,sizeof(long))!=sizeof(long))
		return NULL;

	if(len==0)
		return NULL;

	char* string=(char*)malloc(sizeof(char)*len);

	if(read(fileno(ms_fp),string,len)!=len)
		return NULL;

	return string;
}

void dump_mid_state(FILE* ms_fp)
{
	struct network_data* units_data=flatten_data_bag(units_bag);
	struct unit_info** units = (struct unit_info**)(units_data == NULL ? NULL : units_data->data);
	long units_len = units == NULL ? 0 : units_bag->n_pockets;

	struct units_progress* progress=get_units_progress(units, units_len);

	struct data_bag* state_bag=create_data_bag();
	struct network_data* state_data;

	// Type of file 0 => MID state file | 1 => MID detailed state file

	dump_int(state_bag,0);

	// in_url_len + in_url

	dump_string(state_bag,args->url);

	// fin_url_len + fin_url

	dump_string(state_bag,gl_s_request->url);

	// file_len + file

	dump_string(state_bag,base_unit_info->file);

	// up_file_len + up_file

	if(base_unit_info->up_file==NULL)
		dump_string(state_bag,"");
	else
		dump_string(state_bag,base_unit_info->up_file);

	// content_length

	dump_long(state_bag,atol(gl_s_response->content_length));

	// downloaded_length

	dump_long(state_bag,progress->content_length);

	// number of left over ranges + left over ranges

	struct data_bag* ranges_bag=create_data_bag();

	if(units==NULL || units_len==0)
	{
		dump_long(state_bag,1);

		dump_long(state_bag,0);

		dump_long(state_bag,atol(gl_s_response->content_length)-1);

	}
	else
	{
		long range_counter=0;

		for(long i=0;i<units_len;i++)
		{
			if(units[i]->range->end-units[i]->range->start+1-units[i]->current_size > 0)
			{
				range_counter++;

				dump_long(ranges_bag,units[i]->range->start+units[i]->current_size);

				dump_long(ranges_bag,units[i]->range->end);
			}
		}

		dump_long(state_bag,range_counter);

		if(range_counter!=0)
		{
			place_data(state_bag,flatten_data_bag(ranges_bag));
		}
	}

	// Dump the data

	state_data=flatten_data_bag(state_bag);

	write(fileno(ms_fp),&state_data->len,sizeof(long)); // Structure size

	write(fileno(ms_fp),state_data->data,state_data->len); // Structure data

}

struct ms_entry* process_ms_entry(FILE* ms_fp)
{
	struct ms_entry* en=(struct ms_entry*)calloc(1,sizeof(struct ms_entry));

	en->type=0;

	long en_len=4; // Type of file

	// Initial URL

	en->in_url=extract_string(ms_fp);

	if(en->in_url==NULL)
		return NULL;

	en_len=en_len+sizeof(long)+strlen(en->in_url)+1;

	// Final URL

	en->fin_url=extract_string(ms_fp);

	if(en->fin_url==NULL)
		return NULL;

	en_len=en_len+sizeof(long)+strlen(en->fin_url)+1;

	// File

	en->file=extract_string(ms_fp);

	if(en->file==NULL)
		return NULL;

	en_len=en_len+sizeof(long)+strlen(en->file)+1;

	// Unprocessed File

	en->up_file=extract_string(ms_fp);

	if(en->up_file==NULL)
		return NULL;

	en_len=en_len+sizeof(long)+strlen(en->up_file)+1;

	// Content Length

	if(read(fileno(ms_fp),&en->content_length,sizeof(long))!=sizeof(long))
		return NULL;

	en_len=en_len+sizeof(long);

	// Download Length

	if(read(fileno(ms_fp),&en->downloaded_length,sizeof(long))!=sizeof(long))
		return NULL;

	en_len=en_len+sizeof(long);

	// Number of left over ranges + left over ranges

	if(read(fileno(ms_fp),&en->n_ranges,sizeof(long))!=sizeof(long))
		return NULL;

	en_len=en_len+sizeof(long);

	en->ranges=(struct http_range*)malloc(sizeof(struct http_range)*en->n_ranges);

	for(long i=0;i<en->n_ranges;i++)
	{
		if(read(fileno(ms_fp),&en->ranges[i].start,sizeof(long))!=sizeof(long))
			return NULL;

		en_len=en_len+sizeof(long);

		if(read(fileno(ms_fp),&en->ranges[i].end,sizeof(long))!=sizeof(long))
			return NULL;

		en_len=en_len+sizeof(long);
	}

	en->en_len=en_len;

	return en;
}

void print_ms_entry(struct ms_entry* en)
{
	printf("\n|-->Type: MID state information entry (0)");

	printf("\n|-->Initial URL: %s",en->in_url);

	printf("\n|-->Redirected URL: %s",en->fin_url);

	printf("\n|-->File: %s",en->file);

	printf("\n|-->Unprocessed-File: %s",en->up_file[0]=='\0' ? "-" : en->up_file);

	printf("\n|-->Content-Length: %s",convert_data(en->content_length,0));

	printf("\n|-->Downloaded-Length: %s",convert_data(en->downloaded_length,0));

	printf("\n|-->Number of left over ranges: %ld",en->n_ranges);

	printf("\n|-->Left over ranges:");

	for(long i=0;i<en->n_ranges;i++)
	{
		printf(" (%ld, %ld)",en->ranges[i].start,en->ranges[i].end);

		if(i!=en->n_ranges-1)
			printf(" |");
	}

	printf("\n");
}

void check_ms_entry(struct ms_entry* en)
{

}

#ifdef LIBSSL_SANE
void dump_detailed_mid_state(FILE* ms_fp)
{
	struct network_data* units_data=flatten_data_bag(units_bag);
	struct unit_info** units = (struct unit_info**)(units_data == NULL ? NULL : units_data->data);
	long units_len = units == NULL ? 0 : units_bag->n_pockets;

	struct units_progress* progress=get_units_progress(units, units_len);

	struct data_bag* state_bag=create_data_bag();
	struct network_data* state_data;

	// Type of file 0 => MID state file | 1 => MID detailed state file

	dump_int(state_bag,1);

	// in_url_len + in_url

	dump_string(state_bag,args->url);

	// fin_url_len + fin_url

	dump_string(state_bag,gl_s_request->url);

	// file_len + file

	dump_string(state_bag,base_unit_info->file);

	// up_file_len + up_file

	if(base_unit_info->up_file==NULL)
		dump_string(state_bag,"");
	else
		dump_string(state_bag,base_unit_info->up_file);

	// content_length

	dump_long(state_bag,atol(gl_s_response->content_length));

	// downloaded_length

	dump_long(state_bag,progress->content_length);

	// number of left over ranges + left over ranges

	struct data_bag* ranges_bag=create_data_bag();

	if(units==NULL || units_len==0)
	{
		dump_long(state_bag,1);

		dump_long(state_bag,0);

		dump_long(state_bag,atol(gl_s_response->content_length)-1);

	}
	else
	{
		long range_counter=0;

		for(long i=0;i<units_len;i++)
		{
			if(units[i]->range->end-units[i]->range->start+1-units[i]->current_size > 0)
			{
				range_counter++;

				dump_long(ranges_bag,units[i]->range->start+units[i]->current_size);

				dump_long(ranges_bag,units[i]->range->end);
			}
		}

		dump_long(state_bag,range_counter);

		if(range_counter!=0)
		{
			place_data(state_bag,flatten_data_bag(ranges_bag));
		}
	}

	// Number of Ranges + Ranges along with their hashes

	dump_long(state_bag,progress->n_ranges);

	FILE* fp=NULL;

	if(gl_s_response->content_encoding==NULL)
		fp=fopen(base_unit_info->file,"r");
	else
		fp=fopen(base_unit_info->up_file,"r");


	if(fp==NULL)
	{
		dump_mid_state(ms_fp);
		return;
	}

	unsigned char md_result[MD5_DIGEST_LENGTH];
	long chunk_len;
	long status;

	char f_buf[MAX_TRANSACTION_SIZE > (1+MD5_DIGEST_LENGTH*8/4) ? MAX_TRANSACTION_SIZE : (1+MD5_DIGEST_LENGTH*8/4)];

	struct stat stat_buf;

	fstat(fileno(fp),&stat_buf);

	for(long i=0;i<progress->n_ranges;i++)
	{

		if(progress->ranges[i].start >= stat_buf.st_size)
		{
			dump_mid_state(ms_fp);
			return;
		}

		dump_long(state_bag,progress->ranges[i].start);

		dump_long(state_bag,progress->ranges[i].end);

		chunk_len=progress->ranges[i].end - progress->ranges[i].start + 1;

		if(lseek(fileno(fp),progress->ranges[i].start,SEEK_SET)!=progress->ranges[i].start)
		{
			dump_mid_state(ms_fp);
			return;
		}

		// Determine MD5 hash

		MD5_CTX md_context;

		MD5_Init(&md_context);

		while(1)
		{
			if(chunk_len<0)
			{
				dump_mid_state(ms_fp);
				return;
			}
			else if(chunk_len==0)
				break;

			status=read(fileno(fp),f_buf,sizeof(f_buf) < chunk_len ? sizeof(f_buf) : chunk_len);

			if(status<=0)
			{
				dump_mid_state(ms_fp);
				return;
			}

			chunk_len=chunk_len-status;

			MD5_Update(&md_context,f_buf,status);
		}

		MD5_Final(md_result, &md_context);

		for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
			sprintf(f_buf+2*i,"%02x", md_result[i]);

		dump_string(state_bag,f_buf);
	}

	// Dump the data

	state_data=flatten_data_bag(state_bag);

	write(fileno(ms_fp),&state_data->len,sizeof(long)); // Structure size

	write(fileno(ms_fp),state_data->data,state_data->len); // Structure data

}

struct d_ms_entry* process_detailed_ms_entry(FILE* ms_fp)
{
	struct d_ms_entry* d_en=(struct d_ms_entry*)calloc(1,sizeof(struct d_ms_entry));

	d_en->type=1;

	long d_en_len=4; // Type of file

	// Initial URL

	d_en->in_url=extract_string(ms_fp);

	if(d_en->in_url==NULL)
		return NULL;

	d_en_len=d_en_len+sizeof(long)+strlen(d_en->in_url)+1;

	// Final URL

	d_en->fin_url=extract_string(ms_fp);

	if(d_en->fin_url==NULL)
		return NULL;

	d_en_len=d_en_len+sizeof(long)+strlen(d_en->fin_url)+1;

	// File

	d_en->file=extract_string(ms_fp);

	if(d_en->file==NULL)
		return NULL;

	d_en_len=d_en_len+sizeof(long)+strlen(d_en->file)+1;

	// Unprocessed File

	d_en->up_file=extract_string(ms_fp);

	if(d_en->up_file==NULL)
		return NULL;

	d_en_len=d_en_len+sizeof(long)+strlen(d_en->up_file)+1;

	// Content Length

	if(read(fileno(ms_fp),&d_en->content_length,sizeof(long))!=sizeof(long))
		return NULL;

	d_en_len=d_en_len+sizeof(long);

	// Download Length

	if(read(fileno(ms_fp),&d_en->downloaded_length,sizeof(long))!=sizeof(long))
		return NULL;

	d_en_len=d_en_len+sizeof(long);

	// Number of left over ranges + left over ranges

	if(read(fileno(ms_fp),&d_en->n_left_ranges,sizeof(long))!=sizeof(long))
		return NULL;

	d_en_len=d_en_len+sizeof(long);

	d_en->left_ranges=(struct http_range*)malloc(sizeof(struct http_range)*d_en->n_left_ranges);

	for(long i=0;i<d_en->n_left_ranges;i++)
	{
		if(read(fileno(ms_fp),&d_en->left_ranges[i].start,sizeof(long))!=sizeof(long))
			return NULL;

		d_en_len=d_en_len+sizeof(long);

		if(read(fileno(ms_fp),&d_en->left_ranges[i].end,sizeof(long))!=sizeof(long))
			return NULL;

		d_en_len=d_en_len+sizeof(long);
	}

	// Number of Ranges + Ranges along with their hash

	if(read(fileno(ms_fp),&d_en->n_ranges,sizeof(long))!=sizeof(long))
		return NULL;

	d_en_len=d_en_len+sizeof(long);

	d_en->ranges=(struct http_range*)malloc(sizeof(struct http_range)*d_en->n_ranges);
	d_en->hashes=(char**)malloc(sizeof(char*)*d_en->n_ranges);

	for(long i=0;i<d_en->n_ranges;i++)
	{
		if(read(fileno(ms_fp),&d_en->ranges[i].start,sizeof(long))!=sizeof(long))
			return NULL;

		d_en_len=d_en_len+sizeof(long);

		if(read(fileno(ms_fp),&d_en->ranges[i].end,sizeof(long))!=sizeof(long))
			return NULL;

		d_en_len=d_en_len+sizeof(long);

		d_en->hashes[i]=extract_string(ms_fp);

		d_en_len=d_en_len+sizeof(long)+strlen(d_en->hashes[i])+1;
	}

	d_en->d_en_len=d_en_len;

	return d_en;
}

void print_detailed_ms_entry(struct d_ms_entry* d_en)
{
	printf("\n|-->Type: MID detailed state information entry (1)");

	printf("\n|-->Initial URL: %s",d_en->in_url);

	printf("\n|-->Redirected URL: %s",d_en->fin_url);

	printf("\n|-->File: %s",d_en->file);

	printf("\n|-->Unprocessed-File: %s",d_en->up_file[0]=='\0' ? "-" : d_en->up_file);

	printf("\n|-->Content-Length: %s",convert_data(d_en->content_length,0));

	printf("\n|-->Downloaded-Length: %s",convert_data(d_en->downloaded_length,0));

	printf("\n|-->Number of left over ranges: %ld",d_en->n_left_ranges);

	printf("\n|-->Left over ranges:");

	for(long i=0;i<d_en->n_left_ranges;i++)
	{
		printf(" (%ld, %ld)",d_en->left_ranges[i].start,d_en->left_ranges[i].end);

		if(i!=d_en->n_left_ranges-1)
			printf(" |");
	}

	printf("\n|-->Number of fetched ranges: %ld",d_en->n_ranges);

	printf("\n|-->Fetched ranges:");

	for(long i=0;i<d_en->n_ranges;i++)
	{
		printf(" (%ld, %ld) [%s]",d_en->ranges[i].start,d_en->ranges[i].end,d_en->hashes[i]);

		if(i!=d_en->n_ranges-1)
			printf(" |");
	}

	printf("\n");
}
#endif

void* read_ms_file(char* ms_file,long entry_number,int flag)
{
	FILE* ms_fp=fopen(ms_file,"r");

	if(ms_fp==NULL)
	{
		mid_cond_exit(1,flag==MS_PRINT,"MID: Unable to open the MID state file %s. Exiting...\n\n",ms_file);

		return NULL;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0)
	{
		mid_cond_exit(1,flag==MS_PRINT,"MID: Unable to acquire lock on the MID state file %s. Exiting...\n\n",ms_file);

		return NULL;
	}

	void* en_info=malloc(sizeof(long)+sizeof(int));

	struct ms_entry* en=NULL;
	struct d_ms_entry* d_en=NULL;
	long status;

	long en_counter=1;
	long read_len=0;

	struct stat stat_buf;
	fstat(fileno(ms_fp),&stat_buf);

	while(1)
	{

		if(entry_number) // If a specific entry number is given
		{
			if(entry_number!=en_counter) // Seek all entries before the given entry number
			{

				if((status=read(fileno(ms_fp),en_info,sizeof(long)))==0)
				{
					fprintf(stderr,"\nMID: End of the file, entry number %ld not found. Exiting...\n\n",entry_number);

					goto normal_exit;
				}
				else if(status!=sizeof(long))
				{
					goto fatal_error;
				}

				read_len=read_len+sizeof(long);

				if(read_len+*((long*)(en_info)) > stat_buf.st_size)
					goto fatal_error;

				read_len=read_len+*((long*)(en_info));

				if(lseek(fileno(ms_fp),read_len,SEEK_SET)!=read_len)
					goto fatal_error;

				en_counter++;

				continue;
			}
		}

		if((status=read(fileno(ms_fp),en_info,sizeof(long)+sizeof(int)))==0)
		{
			if(entry_number)
				fprintf(stderr,"\nMID: End of the file, entry number %ld not found. Exiting...\n\n",entry_number);

			goto normal_exit;
		}
		else if(status!=sizeof(long)+sizeof(int))
		{
			goto fatal_error;
		}

		if(*((int*)(en_info+sizeof(long)))==0)
		{
			en=process_ms_entry(ms_fp);

			if(en==NULL || en->en_len != *((long*)en_info))
				goto fatal_error;

			if(flag==MS_PRINT)
			{
				printf("Entry Count: %ld\n",en_counter);
				print_ms_entry(en);
				printf("\n");

				en_counter++;
			}
			else
			{
				flock(fileno(ms_fp),LOCK_UN);
				fclose(ms_fp);

				return (void*)en;
			}
		}
#ifdef LIBSSL_SANE
		else if(*((int*)(en_info+sizeof(long)))==1)
		{
			d_en=process_detailed_ms_entry(ms_fp);

			if(d_en==NULL || d_en->d_en_len != *((long*)en_info))
				goto fatal_error;

			if(flag==MS_PRINT)
			{
				printf("Entry Count: %ld\n",en_counter);
				print_detailed_ms_entry(d_en);
				printf("\n");
				en_counter++;
			}
			else
			{
				flock(fileno(ms_fp),LOCK_UN);
				fclose(ms_fp);

				return (void*)d_en;
			}
		}
#endif
		else
		{
			goto fatal_error;
		}

		if(entry_number) // If a entry number is set exit after printing the information
		{
			goto normal_exit;
		}
	}

	fatal_error:

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	mid_cond_exit(1,flag==MS_PRINT,"MID: %s is broken, errors encountered when processing the MID state file. Exiting...\n\n",ms_file);

	return NULL;

	normal_exit:

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	if(flag==MS_PRINT)
	{
		mid_exit(0);
	}
	else
		return NULL;

}

void clear_ms_entry(char* ms_file,long entry_number,int flag)
{
	if(entry_number<=0)
	{
		if(flag==MS_PRINT)
			fprintf(stderr,"\nMID: Entry number %ld is not valid. Exiting...\n\n");

		return;
	}

	FILE* ms_fp=fopen(ms_file,"r");

	if(ms_fp==NULL) // No file exits
	{
		if(flag==MS_PRINT)
			fprintf(stderr,"\nMID: Unable to open MID state file %s. Exiting...\n\n",ms_file);

		return;
	}

	fclose(ms_fp);

	ms_fp=fopen(ms_file,"r+");

	if(ms_fp==NULL) // Cant open file in append mode
	{
		if(flag==MS_PRINT)
			fprintf(stderr,"\nMID: Unable to open MID state file %s. Exiting...\n\n",ms_file);

		return;
	}

	if(flock(fileno(ms_fp),LOCK_EX)!=0) // Unable to acquire lock
	{
		if(flag==MS_PRINT)
			fprintf(stderr,"\nMID: Unable to acquire lock on MID state file %s. Exiting...\n\n",ms_file);

		return;
	}

	struct stat stat_buf;
	fstat(fileno(ms_fp),&stat_buf);

	void* en_info=malloc(sizeof(long));
	long prev_len=0;
	long read_len=0;
	long status=0;
	long en_counter=0;

	while(en_counter!=entry_number) // Seek to the entry that has to be cleared.
	{
		prev_len=read_len;

		if((status=pread(fileno(ms_fp),en_info,sizeof(long),read_len))==0) // EOF reached, no such entry
		{
			if(flag==MS_PRINT)
				fprintf(stderr,"\nMID: EOF file reached, entry number %ld not found. Exiting...\n\n",entry_number);

			goto normal_return;
		}
		else if(status!=sizeof(long)) // File corrupted, but do nothing.
		{
			if(flag==MS_PRINT)
				fprintf(stderr,"\nMID: %s is not a MID state file. Exiting...\n\n",ms_file);

			goto normal_return;
		}

		read_len=read_len+sizeof(long);

		if(read_len+*((long*)(en_info)) > stat_buf.st_size) // File corrupted, but do nothing.
		{
			if(flag==MS_PRINT)
				fprintf(stderr,"\nMID: %s is not a MID state file. Exiting...\n\n",ms_file);

			goto normal_return;
		}

		read_len=read_len+*((long*)(en_info));

		en_counter++;
	}

	char buf[MAX_TRANSACTION_SIZE];

	while(1) // Start copying data from read_len -> prev_len.
	{

		status=pread(fileno(ms_fp),buf,MAX_TRANSACTION_SIZE,read_len);

		if(status<0)
		{
			if(flag==MS_PRINT)
				fprintf(stderr,"\nMID: Unable to read MID state file %s. Exiting...\n\n",ms_file);

			goto normal_return;
		}
		else if(status==0)
			break;


		read_len=read_len+status;

		if(pwrite(fileno(ms_fp),buf,status,prev_len)!=status) // Cant write to the file.
		{
			if(flag==MS_PRINT)
				fprintf(stderr,"\nMID: Unable to write to MID state file %s. Exiting...\n\n",ms_file);

			goto normal_return;
		}

		prev_len=prev_len+status;
	}

	if(prev_len==0)
	{
		flock(fileno(ms_fp),LOCK_UN);
		fclose(ms_fp);

		remove(ms_file);

		if(flag==MS_PRINT)
			fprintf(stderr,"\nMID: %s has no more entries left. Deleting...\n\n",ms_file);

		return;
	}

	ftruncate(fileno(ms_fp),prev_len);

	normal_return:

	flock(fileno(ms_fp),LOCK_UN);
	fclose(ms_fp);

	return;

}

void init_resume()
{
	char* ms_file=get_ms_filename();
}
