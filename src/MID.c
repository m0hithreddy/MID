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
#include"MID_state.h"
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

#ifdef LIBSSL_SANE
#include<openssl/md5.h>
#endif

struct mid_args* args;

FILE* o_fp=NULL;
FILE* u_fp=NULL;

int handler_registered=0;

struct http_request* gl_s_request=NULL;
struct http_response* gl_s_response=NULL;

struct unit_info* base_unit_info=NULL;

struct data_bag* resume_bag=NULL;

void* entry=NULL;

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
		mid_flag_exit1(1,"MID: Not a HTTP or HTTPS URL. Exiting...\n\n");

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
		mid_flag_exit1(1,"MID: Unable to find the IPV4 address of %s. Exiting...\n\n",purl->host);

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nDNS Resolve:\n\n");
		fprintf(stderr,"->%s IPV4 address: %s\n",purl->host,hostip);
	}

	// Get Network Interfaces Info and Check whether Server is accessible or not

	struct network_interface** net_if=get_network_interfaces();

	if(net_if[0]==NULL)
		mid_flag_exit1(1,"MID: No network-interface found for downloading. Exiting...\n\n");

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
		mid_flag_exit1(1,"MID: Error Checking partial content support. Exiting...\n\n");


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

	// For each network interface check whether server is accessible

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

		if(args->validate_ms)
		{
			check_ms_entry(args->cm_file,args->entry_number,gl_s_request,gl_s_response);
			exit(0);
		}

		net_if_data->data=(char*)net_if[i];
		net_if_data->len=sizeof(struct network_interface);

		place_data(net_if_bag,net_if_data);
	}

	if(gl_s_response==NULL)
		mid_flag_exit1(1,"MID: Error reading server response. Exiting...\n\n");
	else
	{
		if(gl_s_response->status_code[0]=='4')
			mid_flag_exit1(1,"MID: Client Side Error, Status-Code: %s , Status: %s. Exiting...\n\n",gl_s_response->status_code,gl_s_response->status);
		else if(gl_s_response->status_code[0]=='5')
			mid_flag_exit1(1,"MID: Server Side Error, Status-Code: %s , Status: %s. Exiting...\n\n",gl_s_response->status_code,gl_s_response->status);
		else if(gl_s_response->status_code[0]!='2')
			mid_flag_exit1(1,"MID: Unknown error reported by server, Status-Code: %s , Status: %s. Exiting...\n\n",gl_s_response->status_code,gl_s_response->status);
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
		mid_flag_exit1(1,"MID: No suitable network-interface found for downloading. Exiting...\n\n");

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
		mid_flag_exit1(1,"MID: No suitable network-interface found for downloading. Exiting...\n\n");

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
		mid_flag_exit1(1,"MID: No mirrors found for %s. Exiting...\n\n",fin_host);

	/* Base client socket_info structure */

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

	/* Base unit_info structure */

	base_unit_info=(struct unit_info*)calloc(1,sizeof(struct unit_info));

	resume_bag=create_data_bag(); // Checking for possibility of resuming the download.
	int resume_status=0;
	if(pc_flag==1 && !args->no_resume)
		resume_status=init_resume();


	if(!resume_status) // If resume_status ==1 , files already determined.
	{
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
	}

	sigemptyset(&base_unit_info->sync_mask);
	sigaddset(&base_unit_info->sync_mask,SIGRTMIN);

	base_unit_info->p_tid=pthread_self();
	base_unit_info->if_name=ok_net_if[0].name;
	base_unit_info->scheme=fin_scheme;
	base_unit_info->host=fin_host;
	base_unit_info->report_size=(long*)calloc(ok_net_if_len,sizeof(long));
	base_unit_info->report_len=ok_net_if_len;
	base_unit_info->max_unit_retries=args->max_unit_retries;
	base_unit_info->fatal_error=(int*)calloc(1,sizeof(int));
	base_unit_info->resume=1;
	base_unit_info->unit_retry_sleep_time=args->unit_retry_sleep_time;
	base_unit_info->cli_info=base_socket_info;

	base_unit_info->servaddr=create_sockaddr_in(*fin_hostips,fin_port,DEFAULT_HTTP_SOCKET_FAMILY);

	base_unit_info->s_request=(struct http_request*)memndup(gl_s_request,sizeof(struct http_request));

	base_unit_info->range=(struct http_range*)malloc(sizeof(struct http_range));
	base_unit_info->range->start=0;
	base_unit_info->range->end=-1;

	base_unit_info->unit_ranges=create_data_bag();

	if(resume_status)
		finalize_resume();

	/* Printing file names */

	if(!args->quiet_flag && gl_s_response->content_encoding == NULL)
	{
		printf("\nOpening File: %s\n",base_unit_info->file);
	}
	else if(!args->quiet_flag)
	{
		printf("\nOpening Unprocessed File: %s\n",base_unit_info->up_file);
	}

	/* Registering thread to handle signals */

	handler_registered=0;

	struct signal_handler_info* s_hd_info=(struct signal_handler_info*)calloc(1,sizeof(struct signal_handler_info));

	s_hd_info->quit=0;
	s_hd_info->p_tid=pthread_self();

	sigemptyset(&s_hd_info->mask);
	sigaddset(&s_hd_info->mask,SIGINT);
	sigaddset(&s_hd_info->mask,SIGQUIT);
	sigaddset(&s_hd_info->mask,SIGRTMIN);

	if(pthread_sigmask(SIG_BLOCK, &s_hd_info->mask, NULL) != 0)  // Block SIGINT and SIGQUIT and SIGRTMIN (signal used for thread sync)
		mid_flag_exit2(1,"MID: Error initiating the signal handler. Exiting...\n\n");

	pthread_mutex_init(&s_hd_info->lock,NULL);
	pthread_create(&s_hd_info->tid, NULL,signal_handler,(void*)s_hd_info);

	handler_registered=1;

	/* Initiating download */

	struct data_bag* units_bag=create_data_bag();
	struct network_data* n_unit=(struct network_data*)malloc(sizeof(struct network_data));
	struct unit_info** units=NULL;
	long units_len=0;
	struct interface_report* prev=NULL;
	struct interface_report* current=NULL;
	struct units_progress* progress=NULL;
	struct unit_info* new=NULL;
	struct unit_info* idle=NULL;
	struct unit_info* err=NULL;
	time_t start_time;
	long downloaded_length=0;
	long content_length=0;
	struct timespec sleep_time;
	sigset_t sync_mask; // For handling user interrupts
	sigemptyset(&sync_mask);
	sigaddset(&sync_mask,SIGRTMIN);

	// Creating thread for Progress Display;

	struct show_progress_info* s_progress_info;

	if(!args->quiet_flag)
	{
		s_progress_info=(struct show_progress_info*)calloc(1,sizeof(struct show_progress_info));

		s_progress_info->units_bag=units_bag;
		s_progress_info->ifs=ok_net_if;
		s_progress_info->ifs_len=ok_net_if_len;
		s_progress_info->content_length=content_length;
		s_progress_info->sleep_time=args->progress_update_time;
		s_progress_info->detailed_progress=args->detailed_progress;
		sigemptyset(&s_progress_info->sync_mask);
		sigaddset(&s_progress_info->sync_mask,SIGRTMIN);

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

		sleep_time.tv_sec=SCHEDULER_DEFAULT_SLEEP_TIME;
		sleep_time.tv_nsec=0;

		while(1) // Downloading...
		{
			pthread_mutex_lock(&s_hd_info->lock);

			pthread_mutex_unlock(&s_hd_info->lock);

			if(sigtimedwait(&sync_mask,NULL,&sleep_time)!=-1)
				break;

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

		base_unit_info->pc_flag=1;

		//Initiating the first range request

		if(!resume_status)
		{
			new=unitdup(base_unit_info);

			new->range->start=0;
			new->range->end=content_length-1;

			pthread_mutex_init(&new->lock,NULL);
			pthread_create(&new->unit_id,NULL,unit,new);

			// Push first unit info to units_bag

			n_unit->data=(void*)&new;
			n_unit->len=sizeof(struct unit_info*);

			place_data(units_bag,n_unit);
		}

		// Scheduling download across different interfaces

		long if_id=0;
		long hostip_id=0;
		struct unit_info* largest=NULL;
		struct unit_info* update=NULL;
		new=NULL;
		long max_parallel_downloads=args->max_parallel_downloads;

		// Initialize scheduler_info structure.

		struct scheduler_info* sch_info=(struct scheduler_info*)calloc(1,sizeof(struct scheduler_info));

		sch_info->current=NULL;
		sch_info->ifs=ok_net_if;
		sch_info->ifs_len=ok_net_if_len;
		sch_info->max_speed=(long*)calloc(ok_net_if_len,sizeof(long));
		sch_info->max_connections=(long*)calloc(ok_net_if_len,sizeof(long));
		sch_info->max_parallel_downloads=max_parallel_downloads;
		sch_info->sleep_time=SCHEDULER_DEFAULT_SLEEP_TIME;

		sleep_time.tv_sec=sch_info->sleep_time;
		sleep_time.tv_nsec=0;

		while(1) // Downloading....
		{

			units=(struct unit_info**)flatten_data_bag(units_bag)->data;
			units_len=units_bag->n_pockets;

			if(sigtimedwait(&sync_mask,NULL,&sleep_time)!=-1)
			{
				if(s_hd_info->quit) //User interrupt
					break;

				/* else interrupted by paused unit, may be there is decrease in interface utilization.
				 *
				 * NOTE: Since we are using SIGRTMIN and general behavior is to queue all the pending signals,
				 * it is guaranteed main() hears to every of its threads.
				 */

				if(*base_unit_info->fatal_error)
					goto idle_unit;
			}

			current=get_interface_report(units,units_len,ok_net_if,ok_net_if_len,prev);

			progress=get_units_progress(units, units_len);

			if(progress->content_length==content_length) // Download complete
				break;

			sch_info->current=current;

			scheduler(sch_info); // Contact the scheduler

			if_id=sch_info->sch_id;
			sleep_time.tv_sec=sch_info->sleep_time;

			prev=current;

			if(if_id<0)
				continue;

			if(resume_bag->n_pockets!=0)
			{
				update=*((struct unit_info**)(resume_bag->first->data));
				delete_data_pocket(resume_bag,resume_bag->first,DELETE_AT);

				idle=NULL; // Should follow same initializations as that of idle==NULL;

				goto fill_unit;
			}

			idle_unit:

			idle=idle_unit(units,units_len);

			if(idle==NULL && new==NULL)
			{
				new=unitdup(base_unit_info);

				update=new;
			}
			else if(idle==NULL)
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

			largest=largest_unit(units,units_len);

			if(largest==NULL) // If all busy in error recovery
			{
				continue;
			}

			// Break the largest unit into two

			pthread_mutex_lock(&largest->lock);

			long rem_chunk=largest->range->end-largest->range->start+1-largest->current_size;

			update->range->end=largest->range->end;

			if(rem_chunk%2==1)
				largest->range->end=largest->range->start+largest->current_size+rem_chunk/2;
			else
				largest->range->end=largest->range->start+largest->current_size+(rem_chunk/2)-1;

			update->range->start=largest->range->end+1;

			pthread_mutex_unlock(&largest->lock);

			fill_unit:

			// Assign an network-interface

			update->if_name=ok_net_if[if_id].name;
			update->if_id=if_id;

			update->cli_info->address=ok_net_if[if_id].address;
			update->cli_info->sock_opts[0]=create_socket_opt(SOL_SOCKET,SO_BINDTODEVICE,ok_net_if[if_id].name,strlen(ok_net_if[if_id].name));

			// Assign a mirror to download

			hostip_id=(hostip_id+1)%args->max_mirrors;
			update->servaddr=create_sockaddr_in(fin_hostips[hostip_id], fin_port, DEFAULT_HTTP_SOCKET_FAMILY);

			// Resume or initiate the unit

			if(idle==NULL)
			{
				pthread_mutex_init(&update->lock,NULL);
				pthread_create(&update->unit_id,NULL,unit,update);   // Create new download unit

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
				pthread_kill(update->unit_id,SIGRTMIN); // Signal the unit to resume the work.
			}
		}
	}

	/* End of the download */

	time_t end_time=time(NULL);

	suspend_units((struct unit_info**)(flatten_data_bag(units_bag)->data),units_bag->n_pockets); // Suspend all download units.

	if(!args->quiet_flag) // Kill the show_progress thread.
	{
		s_progress_info->quit=1;

		pthread_kill(s_progress_info->tid,SIGRTMIN);
		pthread_join(s_progress_info->tid,NULL);
		pthread_mutex_destroy(&s_progress_info->lock);

	}

	while(resume_bag->n_pockets!=0) // If user interrupted before resuming units, then place them back in units_bag for purpose of storing mid_state.
	{
		new=*((struct unit_info**)resume_bag->first->data);

		n_unit->data=(void*)&new;
		n_unit->len=sizeof(struct unit_info*);

		place_data(units_bag,n_unit);

		delete_data_pocket(resume_bag,resume_bag->first,DELETE_AT);
	}

	// Get the latest download progress

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
		mid_flag_exit3(2,"MID: Error downloading chunk. Exiting...\n\n");

	pthread_mutex_lock(&s_hd_info->lock); // Check if user interrupted the download.
	if(s_hd_info->quit)
	{
		pthread_mutex_unlock(&s_hd_info->lock);
		if(resume_status)
			resave_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
		else
			save_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);

		mid_exit(1);
	}
	pthread_mutex_unlock(&s_hd_info->lock);

	// Handle Content-Encoding

	if(gl_s_response->content_encoding==NULL)
		goto normal_exit;

	if(!args->quiet_flag)
		printf("Decoding the file: %s\n",base_unit_info->up_file);

	struct encoding_info* en_info=determine_encodings(gl_s_response->content_encoding); // Determine the encodings

	if(en_info==NULL)
		mid_flag_exit3(3,"MID: Content-Encoding unknown, not removing the unprocessed file %s. Exiting...\n\n",base_unit_info->up_file);

	if(!args->quiet_flag)
		printf("\nOpening File: %s\n\n",base_unit_info->file);

	char uf_data[MAX_TRANSACTION_SIZE];
	int status;
	int en_status=EN_OK;

	en_info->in=uf_data;

	while(1)
	{
		if(s_hd_info->quit)
		{
			if(resume_status)
				resave_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);
			else
				save_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);

			mid_exit(2);
		}

		status=read(fileno(u_fp),uf_data,MAX_TRANSACTION_SIZE); // Read the unprocessed file.

		if(status<0)
			mid_flag_exit3(2,"MID: Error reading the file %s, not removing the unprocessed file %s. Exiting...\n\n",base_unit_info->up_file,base_unit_info->up_file);
		else if(status==0)
		{
			remove(base_unit_info->up_file); // Processed all data so need of .up file.

			goto normal_exit;
		}

		en_info->in_max=status;
		en_info->in_len=0;

		while(en_info->in_len!=en_info->in_max) // Decode and write the data to the file.
		{
			if(en_status==EN_END)
				mid_flag_exit3(4,"MID: Misc data found at the end of the file %s, not removing the unprocessed file %s. Exiting...\n\n",base_unit_info->up_file,base_unit_info->up_file);

			en_status=handle_encodings(en_info);

			if(en_status!=EN_OK && en_status!=EN_END)
				mid_flag_exit3(2,"MID: Error encountered when decoding the file %s, not removing the unprocessed file %s. Exiting...\n",base_unit_info->up_file,base_unit_info->up_file);

			if(write(fileno(o_fp),en_info->out,en_info->out_len)!=en_info->out_len)
				mid_flag_exit3(2,"\nMID: Error writing to the file %s, not removing the unprocessed file %s. Exiting...\n\n",base_unit_info->file,base_unit_info->up_file);

		}

	}

	normal_exit:

	close_files();
	deregister_handler(s_hd_info);

	if(resume_status)
		resave_mid_state(gl_s_request,gl_s_response,base_unit_info,units_bag,progress);

	mid_exit(0);
}

void* signal_handler(void* v_s_hd_info)
{

	struct signal_handler_info* s_hd_info=(struct signal_handler_info*)v_s_hd_info;

	int err,signo;

	err=sigwait(&s_hd_info->mask, &signo);

	if(err != 0)
		mid_flag_exit2(1,"MID: Error initiating the signal handler. Exiting...\n\n");

	pthread_mutex_lock(&s_hd_info->lock);

	s_hd_info->quit=1;

	pthread_mutex_unlock(&s_hd_info->lock);

	pthread_kill(s_hd_info->p_tid,SIGRTMIN);

	return NULL;
}

void deregister_handler(struct signal_handler_info* s_hd_info)
{
	if(handler_registered && s_hd_info!=NULL)
	{
		pthread_kill(s_hd_info->tid,SIGRTMIN);
		pthread_join(s_hd_info->tid,NULL);
		pthread_mutex_destroy(&s_hd_info->lock);
	}
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

int init_resume()
{
	char* ms_file=get_ms_filename();

	entry=read_ms_entry(ms_file,args->entry_number <=0 ? 1 : args->entry_number,MS_RETURN);

	if(entry==NULL)
	{
		if(args->entry_number > 0)
		{
			mid_cond_print(!args->quiet_flag,"MID: Error retrieving the MS entry structure. Exiting...\n\n");
			exit(1);
		}

		return 0;
	}

	int status;
	struct ms_entry* en;

	if(((struct ms_entry*)entry)->type==0)
	{
		status=validate_ms_entry(((struct ms_entry*)entry),gl_s_request,gl_s_response,MS_SILENT);
		en=(struct ms_entry*)entry;
	}
#ifdef LIBSSL_SANE
	else if(((struct ms_entry*)entry)->type==1)
	{
		status=validate_d_ms_entry((struct d_ms_entry*)entry,gl_s_request,gl_s_response,MS_SILENT);
		en=((struct d_ms_entry*)entry)->en;
	}
#endif
	else
		return 0;


	if(status==-1)
	{
		mid_cond_print(!args->quiet_flag,"MID: Partially downloaded file found...\n\n");

		if(args->force_resume)
			mid_cond_print(!args->quiet_flag,"MID: Fatal error encountered when validating the MS entry, can not resume the download. Exiting...\n\n");
		else
			mid_cond_print(!args->quiet_flag,"MID: Error encountered when validating MS entry, try giving --force-resume flag or check the errors with --validate-ms. Exiting...\n\n");

		exit(1);

	}
	else if(status==1)
	{
		mid_cond_print(!args->quiet_flag,"MID: Force Resuming the download...\n");
	}
	else
	{
		mid_cond_print(!args->quiet_flag,"MID: Partially downloaded file found and is sane. Resuming download...\n\n");
	}


	if(en->file!=NULL && en->file[0]!='\0')
	{
		if((o_fp=fopen(en->file,"r+"))==NULL)
		{
			mid_cond_print(!args->quiet_flag,"MID: Error opening output file %s. Exiting...\n\n",en->file);

			exit(1);
		}

		if(flock(fileno(o_fp),LOCK_EX)!=0)
		{
			mid_cond_print(!args->quiet_flag,"MID: Error acquiring lock on output file %s. Exiting...\n\n",en->file);

			exit(1);
		}
	}

	if(en->up_file!=NULL && en->up_file[0]!='\0')
	{
		if((u_fp=fopen(en->up_file,"r+"))==NULL)
		{
			mid_cond_print(!args->quiet_flag,"MID: Error opening unprocessed file %s. Exiting...\n\n",en->up_file);

			exit(1);
		}

		if(flock(fileno(u_fp),LOCK_EX)!=0)
		{
			mid_cond_print(!args->quiet_flag,"MID: Error acquiring lock on unprocessed file %s. Exiting...\n\n",en->up_file);

			exit(1);
		}
	}

	base_unit_info->file= en->file==NULL || en->file[0]=='\0' ? NULL : en->file;
	base_unit_info->up_file= en->up_file==NULL || en->up_file[0]=='\0' ? NULL : en->up_file;

	return 1;
}

void finalize_resume()
{
	if(entry==NULL)
		return;

	struct http_range* ranges;
	long n_ranges;
	struct ms_entry* en;

	if(((struct ms_entry*)entry)->type==0)
		en=((struct ms_entry*)entry);
#ifdef LIBSSL_SANE
	else if(((struct ms_entry*)entry)->type==1)
		en=((struct d_ms_entry*)entry)->en;
#endif
	else
		return;

	ranges=en->l_ranges;
	n_ranges=en->n_l_ranges;

	if(!n_ranges)
	{
		ranges=(struct http_range*)malloc(sizeof(struct http_range));
		ranges->start=en->content_length;
		ranges->end=en->content_length-1;

		n_ranges=1;
	}

	if(resume_bag==NULL)
		resume_bag=create_data_bag();

	struct unit_info* new;
	struct network_data n_data;



	for(long i=0;i<n_ranges;i++)
	{
		new=unitdup(base_unit_info);

		new->pc_flag=1;

		// Range that need to be fetched

		new->range->start=ranges[i].start;
		new->range->end=ranges[i].end;

		// Already fetched ranges are assigned to the units (one for each) for the purpose of progress indicator

		append_data_pocket(new->unit_ranges,sizeof(long));
		new->unit_ranges->end->len=sizeof(long);

		if(i==0)
			*(long*)new->unit_ranges->end->data=0;
		else
			*(long*)new->unit_ranges->end->data=ranges[i-1].end+1;

		append_data_pocket(new->unit_ranges,sizeof(long));
		new->unit_ranges->end->len=sizeof(long);
		*(long*)new->unit_ranges->end->data=ranges[i].start-1;

		n_data.data=(void*)&new;
		n_data.len=sizeof(struct unit_info*);

		place_data(resume_bag,&n_data);

	}

}
