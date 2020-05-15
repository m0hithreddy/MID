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
#include<sys/time.h>

#ifndef CONFIG_H
#define CONFIG_H
#include"config.h"
#endif

#ifdef LIBSSL_SANE
#include<openssl/md5.h>
#endif

struct mid_args* args=NULL;
FILE* o_fp=NULL;
FILE* u_fp=NULL;
pthread_mutex_t err_lock;
pthread_mutex_t write_lock;
long fatal_error=0;

int handler_registered=0;

struct http_request* gl_s_request=NULL;
struct http_response* gl_s_response=NULL;

struct unit_info* base_unit_info=NULL;

struct mid_bag* resume_bag=NULL;

void* entry=NULL;

int main(int argc, char **argv)
{

	pthread_mutex_init(&err_lock,NULL);
	pthread_mutex_init(&write_lock,NULL);

	/* Parse cmd_line args, config_file args and default_args */

	struct mid_bag* pa_result = create_mid_bag();

	if (parse_mid_args(argv, argc, MID_MODE_READ_DEFAULT_VALUES | MID_MODE_READ_CONF_FILE | \
			MID_MODE_READ_CMD_LINE | MID_MODE_PRINT_HELP, pa_result) != MID_ERROR_NONE) {

		exit(1);
	}

	args = (struct mid_args*) pa_result->first->data;

	/* Handle non download specific actions */

	if (check_mid_args(args) != MID_ERROR_NONE)
		exit(0);

	if( !args->surpass_root_check && getuid()!=0 && geteuid()!=0)
	{
		mid_err("MID: MID is not invoked by root user, falling back to normal download mode. Use -s to surpass the root checks and make MID to bind the interfaces.\n\n");
		args->root_mode=0;
	}
	else
		args->root_mode=1;

	//Parse the URL

	struct parsed_url *purl;
	purl=parse_url(args->url);


	if( purl==NULL || !( !strcmp(purl->scheme,"https") || !strcmp(purl->scheme,"http") ) )
		mid_flag_exit1(1,"\nMID: Unsupported protocol encountered. Exiting...\n\n");

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nURL Information:\n\n");

		fprintf(stderr,"->Scheme: %s\n",purl->scheme);
		fprintf(stderr,"->Host: %s\n",purl->host);
		fprintf(stderr,"->Port: %s\n",purl->port);
		fprintf(stderr,"->PATH: %s\n",purl->path);
		fprintf(stderr,"->Query: %s\n",purl->query);
	}

	/* Handle SIGPIPE signal by blocking it (EPIPE error is received instead) */

	sigset_t sigpipemask;

	sigemptyset(&sigpipemask);
	sigaddset(&sigpipemask, SIGPIPE);

	if(sigprocmask(SIG_BLOCK, &sigpipemask, NULL) != 0)
		mid_flag_exit1(1, "Error blocking SIGPIPE signal. Exiting...\n\n");

	// If SSL enabled then initialize SSL.

#ifdef LIBSSL_SANE
	setup_ssl();
#endif

	// Get Network Interfaces Info and Check whether Server is accessible or not

	struct mid_interface** net_if;

	if(args->root_mode)
	{
		net_if=get_mid_interfaces();
	}
	else
	{
		net_if=(struct mid_interface**)malloc(sizeof(struct mid_interface*)*2);

		net_if[0]=(struct mid_interface*)malloc(sizeof(struct mid_interface));
		net_if[0]->address=NULL;
		net_if[0]->family=AF_INET;
		net_if[0]->name="default";
		net_if[0]->netmask=NULL;

		net_if[1]=NULL;
	}

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

	struct http_request* s_request=(struct http_request*)calloc(1,sizeof(struct http_request));

	s_request->method = "GET";

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
	s_request->custom_headers=args->custom_headers;
	s_request->url=args->url;
	s_request->scheme=purl->scheme;

	struct mid_data* request=create_http_request(s_request);

	if(args->verbose_flag && !args->quiet_flag)
	{
		fprintf(stderr,"\nHTTP Request Message:\n\n");
		fprintf(stderr,"->");
		mid_err("%s",request->data);
	}

	struct mid_bag* net_if_bag=create_mid_bag();

	struct mid_data* net_if_data=(struct mid_data*)malloc(sizeof(struct mid_data));

	// For each network interface check whether server is accessible

	long content_length=-1;
	hashmap* ifs_used = init_hashmap();
	struct hash_token if_token;

	for (int i = 0; net_if[i]!=NULL ; i++)
	{
		s_request->hostip=NULL;

		if((net_if[i]->family != AF_INET && net_if[i]->family != AF_INET6) || \
				(net_if[i]->family == AF_INET && args->ipv4 != 1) || \
				(net_if[i]->family == AF_INET6 && args->ipv6 != 1))
			continue;

		if_token.token = net_if[i]->name;
		if_token.len  = strlen(net_if[i]->name);

		if(get_value(ifs_used, &if_token) != NULL)   /* Interface already passed the test,\
		 include only once (IPv4 or IPv6). */
			continue;

		/* Initiate struct mid_client */

		struct mid_client* mid_cli = create_mid_client(net_if[i], purl);

		if(init_mid_client(mid_cli) != MID_ERROR_NONE)
			continue;

		s_request->hostip=mid_cli->hostip;

		struct mid_bag* http_result = create_mid_bag();

		int http_return = mid_http(mid_cli, request, MID_MODE_SEND_REQUEST | MID_MODE_READ_HEADERS, http_result);

		free_mid_client(&mid_cli);

		if(http_return != MID_ERROR_NONE)
			continue;

		struct mid_data* hdr_data = (struct mid_data*) http_result->end->data;

		if(args->vverbose_flag && !args->quiet_flag)
		{
			fprintf(stderr,"\nHTTP Response Message:\n\n");
			fprintf(stderr,"->");
			mid_err("%s", (char*) hdr_data->data);
		}

		struct mid_bag* fr_result = create_mid_bag();

		if (follow_redirects(s_request, hdr_data, net_if[i], args->max_redirects, MID_MODE_RETURN_S_REQUEST | \
				MID_MODE_RETURN_S_RESPONSE | MID_MODE_FOLLOW_HEADERS, fr_result) != MID_ERROR_NONE)  { /* If error encountered when following
				redirects */

			continue;
		}

		struct http_request* tmp_s_request = (struct http_request*) fr_result->first->data;

		struct http_response* tmp_s_response = (struct http_response*) fr_result->end->data;

		if(gl_s_response==NULL)
		{
			gl_s_response=tmp_s_response;
			gl_s_request=tmp_s_request;
		}

		if(tmp_s_response->status_code==NULL || strcmp(tmp_s_response->status_code,"200"))
			continue;

		gl_s_request=tmp_s_request;
		gl_s_response=tmp_s_response;

		if(args->validate_ms) // If user requested for validation then validate and exit.
		{
			check_ms_entry(args->cm_file,args->entry_number,tmp_s_request,tmp_s_response);
			exit(0);
		}

		long tmp_content_length;  // content_length of this response.

		if(tmp_s_response->accept_ranges==NULL || strcmp(tmp_s_response->accept_ranges,"bytes")!=0 || tmp_s_response->content_length==NULL)
			tmp_content_length=0;
		else
			tmp_content_length=atol(tmp_s_response->content_length);

		if(content_length==-1) // if for the first time, then assign.
			content_length=tmp_content_length;
		else if(content_length!=tmp_content_length) // If not equal, then probably not a static content, fall back to normal download mode.
		{
			content_length=0;
			((struct mid_interface*)net_if_bag->first->data)->address=NULL;  // If in this branch, then net_if_bag->n_pockets>=1
			((struct mid_interface*)net_if_bag->first->data)->family=AF_INET;
			((struct mid_interface*)net_if_bag->first->data)->name="default";
			((struct mid_interface*)net_if_bag->first->data)->netmask=NULL;

			net_if_bag->n_pockets=1;
			break;
		}

		net_if_data->data=(char*)net_if[i];
		net_if_data->len=sizeof(struct mid_interface);

		place_mid_data(net_if_bag,net_if_data);

		if_token.token = net_if[i]->name;
		if_token.len = strlen(net_if[i]->name);

		insert_pair(ifs_used, &if_token, &if_token);
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

	net_if_data=flatten_mid_bag(net_if_bag);

	if(net_if_data==NULL)
		mid_flag_exit1(1,"MID: No suitable network-interface found for downloading. Exiting...\n\n");

	struct mid_interface* ok_net_if=(struct mid_interface*)net_if_data->data;
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

	int pc_flag=content_length ? 1 : 0;

	//Final values for Host Port and URL scheme after redirects

	struct parsed_url* fin_purl=parse_url(gl_s_request->url);

	char* fin_host=strdup(fin_purl->host);
	char* fin_scheme=strdup(fin_purl->scheme);
	long fin_port=atoi((fin_purl->port!=NULL)? fin_purl->port:(!(strcmp(fin_scheme,"http"))? DEFAULT_HTTP_PORT:DEFAULT_HTTPS_PORT));

	/* Base unit_info structure */

	base_unit_info=(struct unit_info*)calloc(1,sizeof(struct unit_info));

	resume_bag=create_mid_bag(); // Checking for possibility of resuming the download.
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
	base_unit_info->scheme=fin_scheme;
	base_unit_info->host=fin_host;
	base_unit_info->report_size=(long*)calloc(ok_net_if_len,sizeof(long));
	base_unit_info->report_len=ok_net_if_len;
	base_unit_info->max_unit_retries=args->max_unit_retries;
	base_unit_info->content_length=content_length;
	base_unit_info->resume=2;
	base_unit_info->pc_flag=pc_flag;
	base_unit_info->unit_retry_sleep_time=args->unit_retry_sleep_time;
	base_unit_info->mid_if=ok_net_if;
	base_unit_info->s_request=(struct http_request*)memndup(gl_s_request,sizeof(struct http_request));
	base_unit_info->range=(struct http_range*)malloc(sizeof(struct http_range));
	base_unit_info->range->start=0;
	base_unit_info->range->end=-1;

	base_unit_info->unit_ranges=create_mid_bag();

	if(resume_status)
		finalize_resume();

	/* Printing file names */

	if(!args->quiet_flag && gl_s_response->content_encoding == NULL)
	{
		printf("\nOpening File: %s\n",base_unit_info->file);

		if(!resume_status && pc_flag)
		{
			ftruncate(fileno(o_fp),content_length);
		}
	}
	else if(!args->quiet_flag)
	{
		printf("\nOpening Unprocessed File: %s\n",base_unit_info->up_file);
		if(!resume_status && pc_flag)
		{
			ftruncate(fileno(u_fp),content_length);
		}
	}

	/* Registering thread to handle signals */

	handler_registered=0;

	struct signal_handler_info* s_hd_info=(struct signal_handler_info*)calloc(1,sizeof(struct signal_handler_info));

	s_hd_info->quit=0;
	s_hd_info->p_tid=pthread_self();

	sigemptyset(&s_hd_info->mask);
	sigaddset(&s_hd_info->mask,SIGINT);
	sigaddset(&s_hd_info->mask,SIGRTMIN);

	if(pthread_sigmask(SIG_BLOCK, &s_hd_info->mask, NULL) != 0)  // Block SIGINT and SIGQUIT and SIGRTMIN (signal used for thread sync)
		mid_flag_exit2(1,"MID: Error initiating the signal handler. Exiting...\n\n");

	pthread_mutex_init(&s_hd_info->lock,NULL);
	pthread_create(&s_hd_info->tid, NULL,signal_handler,(void*)s_hd_info);

	handler_registered=1;

	/* Initiating download */

	struct mid_bag* units_bag=create_mid_bag();
	struct mid_data* n_unit=(struct mid_data*)malloc(sizeof(struct mid_data));
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

		place_mid_data(units_bag,n_unit);

		units=(struct unit_info**)flatten_mid_bag(units_bag)->data;
		units_len=units_bag->n_pockets;

		sleep_time.tv_sec=MID_DEFAULT_SCHEDULER_SLEEP_TIME;
		sleep_time.tv_nsec=0;
		int signo;

		while(1) // Downloading...
		{
			sigwait(&sync_mask,&signo);

			pthread_mutex_lock(&s_hd_info->lock);
			if(s_hd_info->quit)
			{
				pthread_mutex_unlock(&s_hd_info->lock);
				break;
			}
			pthread_mutex_unlock(&s_hd_info->lock);

			pthread_mutex_lock(&err_lock);
			if(fatal_error)
			{
				pthread_mutex_unlock(&err_lock);
				err=handle_unit_errors(units,units_len);

				if(err!=NULL) //fatal error exit.
					break;

				continue;
			}
			pthread_mutex_unlock(&err_lock);

			break; // Download completed and signaled by child.
		}
	}

	else // If range requests are supported
	{
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

			place_mid_data(units_bag,n_unit);
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

		sch_info->ifs=ok_net_if;
		sch_info->ifs_len=ok_net_if_len;
		sch_info->max_parallel_downloads=max_parallel_downloads;
		sch_info->sch_sleep_time=MID_DEFAULT_SCHEDULER_SLEEP_TIME;

		sleep_time.tv_sec=sch_info->sch_sleep_time;
		sleep_time.tv_nsec=0;

		while(1) // Downloading....
		{

			units=(struct unit_info**)flatten_mid_bag(units_bag)->data;
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

				pthread_mutex_lock(&err_lock);
				if(fatal_error)
				{
					pthread_mutex_unlock(&err_lock);
					err=handle_unit_errors(units,units_len);

					if(err!=NULL)
						break;

					continue;
				}
				pthread_mutex_unlock(&err_lock);
			}

			current=get_interface_report(units,units_len,ok_net_if,ok_net_if_len,prev);

			progress=get_units_progress(units, units_len);

			if(progress->content_length==content_length) // Download complete
				break;

			sch_info->ifs_report=current;

			(*args->schd_alg)(sch_info); // Contact the scheduler

			if_id=sch_info->sch_if_id;
			sleep_time.tv_sec=sch_info->sch_sleep_time;

			prev=current;

			if(if_id<0)
				continue;

			if(resume_bag->n_pockets!=0)
			{
				update=*((struct unit_info**)(resume_bag->first->data));
				delete_mid_pocket(resume_bag,resume_bag->first,DELETE_AT);

				idle=NULL; // Should follow same initializations as that of idle==NULL;

				goto fill_unit;
			}

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

			if(rem_chunk<=1)
			{
				pthread_mutex_unlock(&largest->lock);
				continue;
			}

			update->range->end=largest->range->end;

			if(rem_chunk%2==1)
				largest->range->end=largest->range->start+largest->current_size+rem_chunk/2;
			else
				largest->range->end=largest->range->start+largest->current_size+(rem_chunk/2)-1;

			update->range->start=largest->range->end+1;
			pthread_mutex_unlock(&largest->lock);

			fill_unit:

			// Assign an network-interface

			update->if_id=if_id;
			update->mid_if=ok_net_if+if_id;

			// Resume or initiate the unit

			if(idle==NULL)
			{
				pthread_mutex_init(&update->lock,NULL);
				pthread_create(&update->unit_id,NULL,unit,update);   // Create new download unit

				n_unit->data=(void*)&update;
				n_unit->len=sizeof(struct unit_info*);

				if(!args->quiet_flag)
					pthread_mutex_lock(&s_progress_info->lock);

				place_mid_data(units_bag,n_unit);

				if(!args->quiet_flag)
					pthread_mutex_unlock(&s_progress_info->lock);

				new=NULL;
			}
			else
			{
				update->resume=update->resume-1; // decrement by one, unit itself decrements by one making idle->resume=1.
				pthread_kill(update->unit_id,SIGRTMIN); // Signal the unit to resume the work.
			}
		}
	}

	/* End of the download */

	time_t end_time=time(NULL);

	suspend_units((struct unit_info**)(flatten_mid_bag(units_bag)->data),units_bag->n_pockets); // Suspend all download units.

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

		place_mid_data(units_bag,n_unit);

		delete_mid_pocket(resume_bag,resume_bag->first,DELETE_AT);
	}

	// Get the latest download progress

	units=(struct unit_info**)flatten_mid_bag(units_bag)->data;
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
			mid_err("MID: Error retrieving the MS entry structure. Exiting...\n\n");
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
		mid_msg("MID: Partially downloaded file found...\n\n");

		if(args->force_resume)
			mid_err("MID: Fatal error encountered when validating the MS entry, can not resume the download. Exiting...\n\n");
		else
			mid_err("MID: Error encountered when validating MS entry, try giving --force-resume flag or check the errors with --validate-ms. Exiting...\n\n");

		exit(1);

	}
	else if(status==1)
	{
		mid_msg("MID: Force Resuming the download...\n");
	}
	else
	{
		mid_msg("MID: Partially downloaded file found and is sane. Resuming download...\n\n");
	}


	if(en->file!=NULL && en->file[0]!='\0')
	{
		if((o_fp=fopen(en->file,"r+"))==NULL)
		{
			mid_err("MID: Error opening output file %s. Exiting...\n\n",en->file);

			exit(1);
		}

		if(flock(fileno(o_fp),LOCK_EX)!=0)
		{
			mid_err("MID: Error acquiring lock on output file %s. Exiting...\n\n",en->file);

			exit(1);
		}
	}

	if(en->up_file!=NULL && en->up_file[0]!='\0')
	{
		if((u_fp=fopen(en->up_file,"r+"))==NULL)
		{
			mid_err("MID: Error opening unprocessed file %s. Exiting...\n\n",en->up_file);

			exit(1);
		}

		if(flock(fileno(u_fp),LOCK_EX)!=0)
		{
			mid_err("MID: Error acquiring lock on unprocessed file %s. Exiting...\n\n",en->up_file);

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

	if(resume_bag==NULL)
		resume_bag=create_mid_bag();

	struct unit_info* new;
	struct mid_data n_data;

	for(long i=0;i<n_ranges;i++)
	{
		new=unitdup(base_unit_info);

		new->pc_flag=1;

		// Range that need to be fetched

		new->range->start=ranges[i].start;
		new->range->end=ranges[i].end;

		// Already fetched ranges are assigned to the units (one for each) for the purpose of progress indicator

		append_mid_pocket(new->unit_ranges,sizeof(long));
		new->unit_ranges->end->len=sizeof(long);

		if(i==0)
			*(long*)new->unit_ranges->end->data=0;
		else
			*(long*)new->unit_ranges->end->data=ranges[i-1].end+1;

		append_mid_pocket(new->unit_ranges,sizeof(long));
		new->unit_ranges->end->len=sizeof(long);
		*(long*)new->unit_ranges->end->data=ranges[i].start-1;

		n_data.data=(void*)&new;
		n_data.len=sizeof(struct unit_info*);

		place_mid_data(resume_bag,&n_data);
	}

	if(n_ranges==0 || ranges[n_ranges-1].end!=en->content_length-1) // One already fetched range is not included when there is not left over range with range.end==content_length-1.
	{
		new=unitdup(base_unit_info);

		// Range to be fetched. (indeed, no range need to be fetched)

		new->range->start=en->content_length;
		new->range->end=en->content_length-1;

		// Already fetched range (for sake of progress indicator)

		append_mid_pocket(new->unit_ranges,sizeof(long));
		new->unit_ranges->end->len=sizeof(long);

		if(n_ranges==0)
			*(long*)new->unit_ranges->end->data=0;
		else
			*(long*)new->unit_ranges->end->data=ranges[n_ranges-1].end+1;

		append_mid_pocket(new->unit_ranges,sizeof(long));
		new->unit_ranges->end->len=sizeof(long);
		*(long*)new->unit_ranges->end->data=en->content_length-1;

		n_data.data=(void*)&new;
		n_data.len=sizeof(struct unit_info*);

		place_mid_data(resume_bag,&n_data);
	}
}
