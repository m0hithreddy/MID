/*
 * MID_downloader.c
 *
 *  Created on: 14-Mar-2020
 *      Author: Mohith Reddy
 */

#include"MID_unit.h"
#include"MID.h"
#include"MID_structures.h"
#include"MID_http.h"
#include"MID_socket.h"
#include"MID_ssl_socket.h"
#include"MID_functions.h"
#include"MID_interfaces.h"
#include"url_parser.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<limits.h>
#include<time.h>
#include<pthread.h>
#include<signal.h>
#include<sys/signalfd.h>
#include<sys/select.h>
#include<fcntl.h>
#include<errno.h>

void* unit(void* info)
{

	struct unit_info* unit_info=(struct unit_info*)info;

	long retries_count=0;
	int signo;

	struct timespec sleep_time;
	sleep_time.tv_nsec=0;

	int sigfd=signalfd(-1,&unit_info->sync_mask,0);

	if(sigfd<0)
		goto fatal_error;

	char sigbuf[sizeof(struct signalfd_siginfo)];

	while(1)
	{
		/* Sync and retry mechanisms */

		init:

		if(unit_info->quit==1 || (unit_info->resume==3 && unit_info->pc_flag==0)) // Initial checks
			return NULL;

		if(unit_info->err_flag!=0) // If an probably recoverable error encountered
		{
			sigwait(&unit_info->sync_mask,&signo);
			unit_quit();

			pthread_mutex_lock(&unit_info->lock);
			unit_info->err_flag=unit_info->err_flag-1;
			pthread_mutex_unlock(&unit_info->lock);
		}

		if(unit_info->self_repair==1) // An automatic retry mechanism
		{
			sleep_time.tv_sec=unit_info->healing_time;

			sigtimedwait(&unit_info->sync_mask,NULL,&sleep_time);
			unit_quit();

			pthread_mutex_lock(&unit_info->lock);
			unit_info->self_repair=0;
			unit_info->resume=1;
			pthread_mutex_unlock(&unit_info->lock);
		}

		if(unit_info->resume!=1) // Unit is idle, wait for work or termination
		{
			retries_count=0;

			sigwait(&unit_info->sync_mask,&signo);
			unit_quit();

			pthread_mutex_lock(&unit_info->lock);
			unit_info->resume=unit_info->resume-1; // resume the unit.
			pthread_mutex_unlock(&unit_info->lock);
		}

		/* Initializations */

		pthread_mutex_lock(&unit_info->lock);
		time(&unit_info->start_time);
		unit_info->status_code=0;

		// Set the s_request fields

		if(unit_info->pc_flag)
		{
			if(unit_info->range->start >=0 && unit_info->range->end >=0 && unit_info->range->start <= unit_info->range->end)
			{
				char range[HTTP_REQUEST_HEADERS_MAX_LEN];
				sprintf(range,"bytes=%ld-%ld",unit_info->range->start,unit_info->range->end);
				unit_info->s_request->range=range;
			}
			else
			{
				unit_info->resume=3;  // 3 represents idle unit (to handle race  conditions between main and unit)
				pthread_mutex_unlock(&unit_info->lock);

				goto signal_parent;
			}
		}

		unit_info->s_request->method="GET";
		pthread_mutex_unlock(&unit_info->lock);

		/* Create and send the HTTP [range] request */

		struct network_data* request=create_http_request(unit_info->s_request); // create request

		unit_quit();

		int sockfd=open_connection(unit_info->cli_info,unit_info->servaddr); // open connection

		if(sockfd<0)
			goto self_repair;

		unit_quit();

		SSL* ssl;
		int http_flag;

		if(!strcmp(unit_info->scheme,"http")) // send request
		{
			http_flag=1;
			send_http_request(sockfd,request,NULL,JUST_SEND);
		}
		else
		{
			http_flag=0;
			ssl=(SSL*)send_https_request(sockfd,request,unit_info->host,JUST_SEND);

			if(ssl==NULL)
			{
				close(sockfd);
				goto self_repair;
			}
		}

		unit_quit();

		/* Read the response and eat the HTTP response headers */

		struct data_bag* eat_bag=create_data_bag();
		char* eat_buf=(char*)malloc(sizeof(char)*MAX_TRANSACTION_SIZE);
		struct network_data* n_eat_buf=(struct network_data*)malloc(sizeof(struct network_data));

		long status;

		int sock_flags=fcntl(sockfd,F_GETFL);
		fcntl(sockfd,F_SETFL,sock_flags | O_NONBLOCK);  // make the socket non-blocking.

		fd_set m_set,t_set;
		FD_ZERO(&m_set);
		FD_SET(sigfd,&m_set);
		FD_SET(sockfd,&m_set);

		int maxfds=sigfd > sockfd ? sigfd+1 : sockfd+1;

		while(1)
		{
			t_set=m_set;
			select(maxfds,&t_set,NULL,NULL,NULL);

			if(FD_ISSET(sigfd,&t_set))  //user interrupt, break;
			{
				read(sigfd,sigbuf,sizeof(struct signalfd_siginfo));
				break;
			}

			if(http_flag)  // if non encrypted socket.
			{
				status=read(sockfd,eat_buf,MAX_TRANSACTION_SIZE);

				if(status<0)
				{
					if(errno==EWOULDBLOCK)
						continue;
				}
			}
#ifdef LIBSSL_SANE
			else  // encrypted socket.
			{
				status=SSL_read(ssl,eat_buf,MAX_TRANSACTION_SIZE);

				if(status<0)
				{
					int ssl_err=SSL_get_error(ssl,status);

					if(ssl_err==SSL_ERROR_WANT_READ || SSL_ERROR_WANT_WRITE)  // retry the call.
						continue;
					else if(ssl_err==SSL_ERROR_SYSCALL) // EOF occured.
					{
						int rel_err=ERR_get_error();  // check if any real error occured.

						if(rel_err==0)
							status=0;
						else
							status=-1;
					}
					else if(ssl_err==SSL_ERROR_ZERO_RETURN)  // TLS connection closed, may be transport is still up.
						status=0;
					else
						status=-1;
				}
			}
#endif

			if(status<=0)
				break;

			n_eat_buf->data=eat_buf;
			n_eat_buf->len=status;

			place_data(eat_bag,n_eat_buf);

			struct network_data* tmp_n_data=flatten_data_bag(eat_bag);

			if(strlocate(tmp_n_data->data,"\r\n\r\n",0,tmp_n_data->len-1)!=NULL) // read until crlfcrlf is encountered.
				break;
		}

		unit_quit();

		if(status<0)
		{
			close(sockfd);
			goto self_repair;
		}


		n_eat_buf=flatten_data_bag(eat_bag);

		struct http_response* s_response=parse_http_response(n_eat_buf); //parse the partial response.

		if(s_response==NULL)
		{
			close(sockfd);
			goto self_repair;
		}

		pthread_mutex_lock(&unit_info->lock);
		unit_info->status_code=atoi(s_response->status_code);

		if(s_response->status_code[0]!='2') // check if response header is valid.
		{
			unit_info->err_flag=2;  // err_flag 2 represents main() supervision in dealing with error,
			pthread_mutex_unlock(&unit_info->lock);

			pthread_mutex_lock(&err_lock);
			fatal_error=fatal_error+1;
			pthread_mutex_unlock(&err_lock);

			close(sockfd);
			goto signal_parent;

		}
		pthread_mutex_unlock(&unit_info->lock);

		/* Get the file and, write the over eaten data and remaining download data to the file */

		FILE* fp;

		if(s_response->content_encoding!=NULL)
			fp=u_fp;
		else
			fp=o_fp;

		char* data_buf=eat_buf; // over eaten data.
		memcpy(data_buf,s_response->body->data,s_response->body->len);
		status=s_response->body->len;

		pthread_mutex_lock(&unit_info->lock); // Initialize fetched unit range entries (range.start,range.start-1)
		append_data_pocket(unit_info->unit_ranges,sizeof(long));
		unit_info->unit_ranges->end->len=sizeof(long);
		*((long*)unit_info->unit_ranges->end->data)=unit_info->range->start;

		append_data_pocket(unit_info->unit_ranges,sizeof(long));
		unit_info->unit_ranges->end->len=sizeof(long);
		*((long*)unit_info->unit_ranges->end->data)=unit_info->range->start-1;
		pthread_mutex_unlock(&unit_info->lock);

		struct encoding_info* en_info=determine_encodings(s_response->transfer_encoding); // determine the transfer encoding used.
		if(en_info==NULL)
		{
			close(sockfd);
			goto fatal_error;
		}

		int en_status=EN_OK;
		long rem_download=0;
		long wr_status=0;

		while(1) // download data...
		{

			pthread_mutex_lock(&unit_info->lock);
			unit_info->report_size[unit_info->if_id]=unit_info->report_size[unit_info->if_id]+status;
			pthread_mutex_unlock(&unit_info->lock);

			if(unit_info->quit)
				break;

			en_info->in=data_buf;
			en_info->in_len=0;
			en_info->in_max=status;

			while(en_info->in_len!=en_info->in_max) //handle encodings
			{

				if(en_status==EN_END) // if encode_handler ended but still more data is given.
				{
					close(sockfd);
					goto fatal_error;
				}

				en_status=handle_encodings(en_info);

				if(en_status!=EN_OK && en_status!=EN_END) // error reported by encode_handler.
				{
					close(sockfd);
					goto fatal_error;
				}

				pthread_mutex_lock(&unit_info->lock); // write the data to file, and update unit_info fields.
				rem_download=unit_info->range->end-unit_info->range->start+1-unit_info->current_size;

				pthread_mutex_lock(&write_lock);
				wr_status=pwrite(fileno(fp),en_info->out,rem_download < en_info->out_len && unit_info->pc_flag ? rem_download : en_info->out_len,unit_info->range->start+unit_info->current_size);
				pthread_mutex_unlock(&write_lock);

				unit_info->current_size=unit_info->current_size+wr_status;
				*((long*)unit_info->unit_ranges->end->data)=*((long*)unit_info->unit_ranges->end->data)+wr_status;

				if(unit_info->pc_flag)
				{
					if(unit_info->current_size==unit_info->range->end-unit_info->range->start+1) // check if requested range is fetched.
					{
						pthread_mutex_unlock(&unit_info->lock);
						goto end_download;
					}
				}
				pthread_mutex_unlock(&unit_info->lock);
			}

			read_socket:

			t_set=m_set;
			select(maxfds,&t_set,NULL,NULL,NULL); // read when ready.

			if(FD_ISSET(sigfd,&t_set))
			{
				read(sigfd,sigbuf,sizeof(struct signalfd_siginfo));
				break;
			}

			if(http_flag)
			{
				status=read(sockfd,data_buf,MAX_TRANSACTION_SIZE);

				if(status<0)
				{
					if(errno==EWOULDBLOCK)
						goto read_socket;
				}
			}
#ifdef LIBSSL_SANE
			else
			{
				status=SSL_read(ssl,data_buf,MAX_TRANSACTION_SIZE);

				if(status<0)
				{
					int ssl_err=SSL_get_error(ssl,status);

					if(ssl_err==SSL_ERROR_WANT_READ || SSL_ERROR_WANT_WRITE)
						goto read_socket;
					else if(ssl_err==SSL_ERROR_SYSCALL)
					{
						int rel_err=ERR_get_error();

						if(rel_err==0)
							status=0;
						else
							status=-1;
					}
					else if(ssl_err==SSL_ERROR_ZERO_RETURN)
						status=0;
					else
						status=-1;
				}
			}
#endif

			if(status<=0)
				break;

		}

		/* End of the download make unit ready for handling next download or exit */

		end_download:

		close(sockfd);

		pthread_mutex_lock(&unit_info->lock);
		unit_info->range->start=*((long*)unit_info->unit_ranges->end->data)+1; // Update left over range entries.
		unit_info->current_size=0;

		if(unit_info->quit) // if requested to quit.
		{
			pthread_mutex_unlock(&unit_info->lock);
			return NULL;
		}

		if(unit_info->current_size<unit_info->range->end-unit_info->range->start+1) // check for incomplete download.
		{
			pthread_mutex_unlock(&unit_info->lock);
			goto self_repair;
		}

		unit_info->resume=3; // make the unit idle and signal parent.
		pthread_mutex_unlock(&unit_info->lock);

		signal_parent:

		pthread_kill(unit_info->p_tid,SIGRTMIN);
	}

	self_repair:

	pthread_mutex_lock(&unit_info->lock);
	if(retries_count<unit_info->max_unit_retries)
	{
		retries_count++;
		unit_info->self_repair=1;
		unit_info->healing_time=unit_info->unit_retry_sleep_time;
		pthread_mutex_unlock(&unit_info->lock);

		goto init;
	}

	unit_info->err_flag=2;
	pthread_mutex_unlock(&unit_info->lock);

	pthread_mutex_lock(&err_lock);
	fatal_error=fatal_error+1;
	pthread_mutex_unlock(&err_lock);

	pthread_kill(unit_info->p_tid,SIGRTMIN);
	return NULL;

	fatal_error:

	pthread_mutex_lock(&unit_info->lock);
	unit_info->err_flag=2;
	pthread_mutex_unlock(&unit_info->lock);

	pthread_mutex_lock(&err_lock);
	fatal_error=fatal_error+1;
	pthread_mutex_unlock(&err_lock);

	pthread_kill(unit_info->p_tid,SIGRTMIN);
	return NULL;
}

struct unit_info* handle_unit_errors(struct unit_info** units,long units_len)
{
	if(units==NULL)
		return NULL;

	for(long i=0;i<units_len && units[i]!=NULL;i++)
	{
		pthread_mutex_lock(&units[i]->lock);
		if(units[i]->err_flag==0)
		{
			pthread_mutex_unlock(&units[i]->lock);
			continue;
		}

		if(units[i]->status_code==503) // Service Temporarily Unavailable
		{
			units[i]->err_flag=units[i]->err_flag-1;
			units[i]->self_repair=1;
			units[i]->healing_time=ERR_CODE_503_HEALING_TIME;
			pthread_mutex_unlock(&units[i]->lock);

			pthread_kill(units[i]->unit_id,SIGRTMIN);

			pthread_mutex_lock(&err_lock);
			fatal_error=fatal_error-1;
			pthread_mutex_unlock(&err_lock);

			return NULL;
		}
		else
		{
			pthread_mutex_unlock(&units[i]->lock);

			pthread_mutex_lock(&err_lock);
			fatal_error=fatal_error-1;
			pthread_mutex_unlock(&err_lock);

			return units[i];
		}
	}

	return NULL;
}

struct interface_report* get_interface_report(struct unit_info** units,long units_len,struct network_interface* net_if,long if_len,struct interface_report* prev)
{
	if(net_if==NULL || if_len==0)
		return NULL;

	struct interface_report* if_report=(struct interface_report*)calloc(if_len,sizeof(struct interface_report));

	for(int i=0;i<if_len;i++)
	{

		if_report[i].if_name=net_if[i].name;
		if_report[i].if_id=i;
		if_report[i].time=time(NULL);

		if(prev!=NULL)
		{
			long time_spent=if_report[i].time-prev[i].time;

			if(time_spent>0)
				if_report[i].speed=-prev[i].downloaded/time_spent;
		}

	}

	long if_id;

	for(int i=0;i<units_len;i++)
	{

		pthread_mutex_lock(&units[i]->lock);

		if(units[i]->resume==1)
			if_report[units[i]->if_id].connections=if_report[units[i]->if_id].connections+1;

		pthread_mutex_unlock(&units[i]->lock);

		for(int j=0;j<if_len;j++)
		{
			pthread_mutex_lock(&units[i]->lock);

			if_report[j].downloaded=if_report[j].downloaded+units[i]->report_size[j];

			if(prev!=NULL)
			{
				long time_spent=if_report[j].time-prev[j].time;

				if(time_spent>0)
					if_report[j].speed=if_report[j].speed+(units[i]->report_size[j])/time_spent;
			}
			else
			{
				long time_spent=if_report[j].time-units[i]->start_time;

				if(time_spent>0)
					if_report[j].speed=if_report[j].speed+(units[i]->report_size[j])/time_spent;
			}

			pthread_mutex_unlock(&units[i]->lock);
		}

	}

	return if_report;
}

struct unit_info* largest_unit(struct unit_info** units,long units_len)
{
	if(units==NULL)
		return NULL;

	struct unit_info* largest=NULL;
	long current=0;

	for(int i=0;i<units_len;i++)
	{
		pthread_mutex_lock(&units[i]->lock);
		if(units[i]->self_repair==1 || units[i]->err_flag!=0 || units[i]->resume!=1)
		{
			pthread_mutex_unlock(&units[i]->lock);
			continue;
		}

		long left_over=units[i]->range->end-units[i]->range->start+1-units[i]->current_size;

		if(left_over>current && left_over>=args->unit_break)
		{
			current=left_over;
			largest=units[i];
		}
		pthread_mutex_unlock(&units[i]->lock);
	}

	return largest;
}

struct unit_info* idle_unit(struct unit_info** units,long units_len)
{
	if(units==NULL)
		return NULL;

	for(long i=0;i<units_len;i++)
	{
		pthread_mutex_lock(&units[i]->lock);
		if(units[i]->resume==3)
		{
			pthread_mutex_unlock(&units[i]->lock);
			return units[i];
		}
		pthread_mutex_unlock(&units[i]->lock);
	}

	return NULL;
}

void scheduler(struct scheduler_info* sch_info)
{

	sch_info->sleep_time=SCHEDULER_DEFAULT_SLEEP_TIME;

	if( sch_info->current==NULL || sch_info->ifs==NULL || sch_info->ifs_len<=0 )
	{
		sch_info->sch_id=-1;
		return;
	}

	long t_conn=0;

	for(long i=0;i<sch_info->ifs_len;i++)
	{
		t_conn=t_conn+sch_info->current[i].connections;
	}

	if(t_conn >= sch_info->max_parallel_downloads)
	{
		sch_info->sch_id=-1;
		goto maxs_update;
	}

	if(!sch_info->probing_done && sch_info->sch_id==-1) // First scheduling decision
	{
		sch_info->sch_id=0;
		goto maxs_update;
	}

	if(!sch_info->probing_done)
	{
		long sch_id=sch_info->sch_id;

		if( 2*sch_info->max_speed[sch_id] <= sch_info->current[sch_id].speed || sch_info->max_speed[sch_id] + SCHEDULER_THRESHOLD_SPEED <= sch_info->current[sch_id].speed )
		{
			goto maxs_update;
		}
		else
		{
			sch_id++;

			if(sch_id<sch_info->ifs_len)
				sch_info->sch_id=sch_id;
			else
			{
				sch_info->sch_id=-1;
				sch_info->probing_done=1;
			}

			goto maxs_update;
		}
	}
	else
	{

		for(long i=0;i<sch_info->ifs_len;i++)
		{
			if( sch_info->current[i].speed + (sch_info->max_speed[i]>2*SCHEDULER_THRESHOLD_SPEED ? SCHEDULER_THRESHOLD_SPEED:sch_info->max_speed[i]/2 ) < sch_info->max_speed[i] && sch_info->current[i].connections <= sch_info->max_connections[i])
			{
				sch_info->sch_id=i;

				goto maxs_update;
			}
		}

		sch_info->sch_id=-1;

		goto maxs_update;
	}

	maxs_update:

	for(long i=0;i<sch_info->ifs_len;i++)
	{
		if(sch_info->max_speed[i] < sch_info->current[i].speed)
		{
			sch_info->max_speed[i]=sch_info->current[i].speed;

			if(sch_info->max_connections[i] < sch_info->current[i].connections)
				sch_info->max_connections[i]=sch_info->current[i].connections;
		}
	}

	return;
}

long suspend_units(struct unit_info** units,long units_len)
{
	if(units==NULL)
		return 0;

	long counter=0;

	for(long i=0;i<units_len;i++)
	{
		units[i]->quit=1;
		pthread_kill(units[i]->unit_id,SIGRTMIN);
		pthread_join(units[i]->unit_id,NULL);
		pthread_mutex_destroy(&units[i]->lock);
		counter++;
	}

	return counter;
}

void* compare_units_progress(void* unit_a,void* unit_b)
{
	int* res=(int*)malloc(sizeof(int));

	if(((struct http_range*)unit_a)->start<((struct http_range*)unit_b)->start)
	{
		*res=1;
		return (void*)res;
	}

	*res=0;
	return (void*)res;
}

struct units_progress* merge_units_progress(struct units_progress* progress)
{

	if(progress==NULL || progress->n_ranges==0 || progress->ranges==NULL)
		return NULL;

	sort((void*)progress->ranges,sizeof(struct http_range),0,progress->n_ranges-1,compare_units_progress);

	struct data_bag* u_bag=create_data_bag();

	append_data_pocket(u_bag,sizeof(long));
	u_bag->end->len=sizeof(long);
	*(long*)u_bag->end->data=progress->ranges[0].start;

	append_data_pocket(u_bag,sizeof(long));
	u_bag->end->len=sizeof(long);
	*(long*)u_bag->end->data=progress->ranges[0].end;

	for (long i = 1 ; i < progress->n_ranges; i++)
	{
		if (*(long*)u_bag->end->data < progress->ranges[i].start)
		{
			append_data_pocket(u_bag,sizeof(long));
			u_bag->end->len=sizeof(long);
			*(long*)u_bag->end->data=progress->ranges[i].start;

			append_data_pocket(u_bag,sizeof(long));
			u_bag->end->len=sizeof(long);
			*(long*)u_bag->end->data=progress->ranges[i].end;

		}

		else if (*(long*)u_bag->end->data < progress->ranges[i].end)
		{
			*(long*)u_bag->end->data = progress->ranges[i].end;
		}
	}

	struct units_progress* u_progress=(struct units_progress*)malloc(sizeof(struct units_progress));

	u_progress->ranges=(struct http_range*)flatten_data_bag(u_bag)->data;
	u_progress->n_ranges=u_bag->n_pockets/2;

	u_progress->content_length=LONG_MIN;

	return u_progress;

}

struct units_progress* get_units_progress(struct unit_info** units,long units_len)
{
	struct units_progress* progress=(struct units_progress*)calloc(1,sizeof(struct units_progress));

	if(units==NULL || units_len==0 )
		return progress;

	struct data_bag* ranges_bag=create_data_bag();

	long ranges_counter=0;

	for(long i=0;i<units_len;i++)
	{
		pthread_mutex_lock(&units[i]->lock);

		place_data(ranges_bag,flatten_data_bag(units[i]->unit_ranges));
		ranges_counter=ranges_counter+units[i]->unit_ranges->n_pockets/2;

		pthread_mutex_unlock(&units[i]->lock);

	}

	struct network_data* n_ranges=flatten_data_bag(ranges_bag);

	if(n_ranges==NULL)
		return progress;

	progress->ranges=(struct http_range*)n_ranges->data;
	progress->n_ranges=ranges_counter;
	progress->content_length=LONG_MIN;

	struct units_progress* u_progress=merge_units_progress(progress);

	if(u_progress==NULL || u_progress->ranges==NULL)
	{
		memset(progress,0,sizeof(struct units_progress));
		return progress;
	}

	long content_length=0;

	for(long i=0;i<u_progress->n_ranges;i++)
	{
		content_length=content_length+u_progress->ranges[i].end-u_progress->ranges[i].start+1;
	}

	u_progress->content_length=content_length;

	return u_progress;
}

void skip_progress_text(long count)
{
	for(long i=0;i<count;i++)
	{
		printf("\n");
	}

}

void* show_progress(void* s_progress_info)
{

	struct show_progress_info* p_info=(struct show_progress_info*)s_progress_info;

	struct interface_report* current=NULL;
	struct interface_report* prev=NULL;
	struct units_progress* progress=NULL;
	struct unit_info** units=NULL;
	struct timespec sleep_time;

	sleep_time.tv_sec=p_info->sleep_time;
	sleep_time.tv_nsec=0;

	long units_len=0;
	long ifs_len=0;

	long quit_flag=0;

	char* bar1=(char*)malloc(sizeof(char)*153);
	memset(bar1,' ',153);
	bar1[0]='|';
	bar1[151]='|';
	bar1[152]='\0';

	char* bar2=(char*)malloc(sizeof(char)*153);

	long bar_status=1;
	int bar_flag=1;
	long marker_start;
	long marker_end;

	while(1)
	{

		if(quit_flag)
		{
			if(p_info->detailed_progress)
				skip_progress_text(ifs_len+9);
			else
				skip_progress_text(5);

			return NULL;
		}

		sigtimedwait(&p_info->sync_mask,NULL,&sleep_time);

		if(p_info->quit==1)
				quit_flag=1;

		pthread_mutex_lock(&p_info->lock);

		units=(struct unit_info**)flatten_data_bag(p_info->units_bag)->data;
		units_len=p_info->units_bag->n_pockets;

		current=get_interface_report(units, units_len, p_info->ifs, p_info->ifs_len, prev);
		progress=get_units_progress(units, units_len);
		ifs_len=p_info->ifs_len;
		prev=current;

		pthread_mutex_unlock(&p_info->lock);

		if(current==NULL)
		{
			continue;
		}

		long t_conc=0;
		long t_speed=0;
		long t_cont=0;

		for(long i=0;i<ifs_len;i++)
		{
			t_conc=t_conc+current[i].connections;
			t_speed=t_speed+current[i].speed;
			t_cont=t_cont+current[i].downloaded;
		}

		// Progress Status Bar

		long marker_eq=p_info->content_length/150;

		if(p_info->content_length==0 || marker_eq==0)
		{

			memcpy(bar2,bar1,153);

			bar2[bar_status]='+';

			printf("\n");
			printf("%s",bar2);
			printf("\n");

			if(bar_flag)
				bar_status++;
			else
				bar_status--;

			if(bar_status==150)
				bar_flag=0;

			if(bar_status==1)
				bar_flag=1;
		}
		else
		{
			memcpy(bar2,bar1,153);


			for(long i=0;i<progress->n_ranges;i++)
			{
				marker_start=1+progress->ranges[i].start/marker_eq + (progress->ranges[i].start%marker_eq == 0 ? 0 : 1 );
				marker_end=1+progress->ranges[i].end/marker_eq;

				if(marker_end>150)
					marker_end=150;

				for(long j=marker_start;j<=marker_end;j++)
				{
					bar2[j]='+';
				}
			}

			printf("\n");
			printf("%s",bar2);
			printf("\n");
		}

		if(p_info->detailed_progress)
		{
			printf("\n");

			for(int i=0;i<152;i++)
				printf("-");

			printf("\n");

			printf("| %-23s | %-20s | %-26s | %-29s | %-38s |","Network Interface","Interface ID","Number of Connections","Download Speed","Data Fetched");
			printf("\n");

			for(long i=0;i<ifs_len;i++)
			{
				printf("| %-23s | %-20ld | %-26ld | %-29s | %-38s |",current[i].if_name,current[i].if_id,current[i].connections,convert_speed(current[i].speed),convert_data(current[i].downloaded,0));
				printf("\n");
			}


			for(int i=0;i<152;i++)
				printf("-");

			printf("\n");
		}

		printf("\n\n");

		printf("Total Connections = %-4ld Total Speed = %-13s Total Downloaded = %-34s %40s left",t_conc,convert_speed(t_speed),convert_data(progress->content_length,p_info->content_length),p_info->content_length==0 ? "unknown" : t_speed==0 ? "inf":convert_time((p_info->content_length-progress->content_length)/t_speed));

		printf("\n");

		fflush(stdout);

		printf("\r");

		if(p_info->detailed_progress)
		{

			for(long i=0;i<ifs_len;i++)
			{
				printf("\e[A");
			}
			printf("\e[A\e[A\e[A\e[A");
		}

		printf("\e[A\e[A\e[A\e[A\e[A");

		pthread_mutex_unlock(&p_info->lock);
	}


}

char* convert_speed(long speed)
{
	char* speed_str=(char*)malloc(sizeof(char)*30);

	if(speed<1024)
	{
		sprintf(speed_str,"%ld B/s",speed);
	}
	else if(speed<1024*1024)
	{
		sprintf(speed_str,"%.2lf KiB/s",((double)speed)/1024);
	}
	else if(speed<1024*1024*1024)
	{
		sprintf(speed_str,"%.2lf MiB/s",((double)speed)/(1024*1024));
	}
	else
	{
		sprintf(speed_str,"%.2lf GiB/s",((double)speed)/(1024*1024*1024));
	}

	return speed_str;
}

char* convert_data(long data,long total_data)
{
	char* data_str=(char*)malloc(sizeof(char)*66);

	if(data<1024)
	{
		if(total_data!=0)
			sprintf(data_str,"%ld B/%s (%.2lf \%)",data,convert_data(total_data,0),(((double)data)/total_data)*100);
		else
			sprintf(data_str,"%ld B",data);
	}
	else if(data<1024*1024)
	{
		if(total_data!=0)
			sprintf(data_str,"%.2lf KiB/%s (%.2lf \%)",((double)data)/1024,convert_data(total_data,0),(((double)data)/total_data)*100);
		else
			sprintf(data_str,"%.2lf KiB",((double)data)/1024);
	}
	else if(data<1024*1024*1024)
	{
		if(total_data!=0)
			sprintf(data_str,"%.2lf MiB/%s (%.2lf \%)",((double)data)/(1024*1024),convert_data(total_data,0),(((double)data)/total_data)*100);
		else
			sprintf(data_str,"%.2lf MiB",((double)data)/(1024*1024));
	}
	else
	{
		if(total_data!=0)
			sprintf(data_str,"%.2lf GiB/%s (%.2lf \%)",((double)data)/(1024*1024*1024),convert_data(total_data,0),(((double)data)/total_data)*100);
		else
			sprintf(data_str,"%.2lf GiB",((double)data)/(1024*1024*1024));
	}

	return data_str;
}

char* convert_time(long sec)
{
	long min=sec/60;
	long r_sec=sec%60;
	long hr=min/60;
	long r_min=min%60;
	long day=hr/24;
	long r_hr=hr%24;
	long mon=day/30;
	long r_day=day%30;
	long yr=mon/30;
	long r_mon=mon%30;

	char* time_str=(char*)malloc(sizeof(char)*41);

	if(min==0)
	{
		sprintf(time_str,"%ld sec",r_sec);
	}
	else if(hr==0)
	{
		sprintf(time_str,"%ld min:%ld sec",r_min,r_sec);
	}
	else if(day==0)
	{
		sprintf(time_str,"%ld hr:%ld min:%ld sec",r_hr,r_min,r_sec);
	}
	else if(mon==0)
	{
		sprintf(time_str,"%ld day:%ld hr:%ld min:%ld sec",r_day,r_hr,r_min,r_sec);
	}
	else if(yr==0)
	{
		sprintf(time_str,"%ld mon:%ld day:%ld hr:%ld min:%ld sec",r_mon,r_day,r_hr,r_min,r_sec);
	}
	else
	{
		sprintf(time_str,"%ld yr:%ld mon:%ld day:%ld hr:%ld min:%ld sec",yr,r_mon,r_day,r_hr,r_min,r_sec);
	}

	return time_str;
}

struct unit_info* unitdup(struct unit_info* src)
{
	struct unit_info* new=(struct unit_info*)memndup(src,sizeof(struct unit_info));

	// No the exact dup!! Only replicating some sensitive fields (dependent on program logic !)

	new->report_size=(long*)calloc(src->report_len,sizeof(long));

	new->cli_info=(struct socket_info*)memndup(src->cli_info,sizeof(struct socket_info));

	new->cli_info->sock_opts=(struct socket_opt*)memndup(src->cli_info->sock_opts,sizeof(struct socket_opt)*2);

	new->s_request=(struct http_request*)memndup(src->s_request,sizeof(struct http_request));

	new->range=(struct http_range*)malloc(sizeof(struct http_range));

	new->unit_ranges=create_data_bag();

	return new;
}

