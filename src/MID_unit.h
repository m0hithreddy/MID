/*
 * MID_downloader.h
 *
 *  Created on: 14-Mar-2020
 *      Author: Mohith Reddy
 */

#ifndef MID_UNIT_H_
#define MID_UNIT_H_

#define UNIT_REPORT_RETURN_HASHMAP 00
#define UNIT_REPORT_RETURN_IF_REPORT 01
#define MIN_UNIT_SLEEP_TIME 1
#define MAX_UNIT_SLEEP_TIME 5
#define MIN_SCHED_SLEEP_TIME 3
#define MAX_SCHED_SLEEP_TIME 7
#define MAX_PARALLEL_DOWNLOADS 10
#define UNIT_BREAK_THRESHOLD_SIZE 102400
#define MAX_UNIT_RETRIES 3
#define UNIT_RETRY_SLEEP_TIME 5
#define ERR_CODE_503_HEALING_TIME 10
#define PROGRESS_UPDATE_TIME 1
#define SCHEDULER_DEFAULT_SLEEP_TIME 2
#define SCHEDULER_THRESHOLD_SPEED 2097152

#include"MID_http.h"
#include"MID_socket.h"
#include"MID_interfaces.h"
#include<netinet/in.h>
#include<time.h>
#include<pthread.h>

#define unit_quit()\
	do\
	{\
		if(unit_info->quit)\
		{\
			if(sockfd>=0)\
			{\
				close(sockfd);\
			}\
			if(sigfd>=0)\
			{\
				close(sigfd);\
			}\
			return NULL;\
		}\
	}while(0)

struct unit_info
{
	pthread_t unit_id;
	pthread_mutex_t lock;
	sigset_t sync_mask;
	pthread_t p_tid;
	char* file;
	char* up_file;
	char* if_name;
	char* scheme;
	char* host;
	long if_id;
	long current_size; // Amount downloaded in a given range
	long* report_size; // Total data fetched by unit from each interface, to compute if_report;
	long report_len;
	long max_unit_retries;
	long content_length;
	int resume;   // {4,3}=>idle states to be handled by main and unit ; {2}=>verifying phase for (206 or 200 HTTP response code) ; {1}=>actual downloading ;
	int quit;
	int pc_flag;
	int err_flag;
	int self_repair;
	int status_code;
	int transfer_encoding;
	long unit_retry_sleep_time;
	long healing_time;
	time_t start_time;
	struct socket_info* cli_info;
	struct http_request* s_request;
	struct http_range* range;
	struct mid_bag* unit_ranges; // HTTP ranges downloaded by unit
};

struct interface_report
{
	char* if_name;
	long if_id;
	long speed; // bytes/sec
	long downloaded; //bytes
	long connections;
	time_t time;
};

struct units_progress
{
	struct http_range* ranges;
	long n_ranges;
	long content_length;
};

struct show_progress_info
{
	pthread_t tid;
	pthread_mutex_t lock;
	sigset_t sync_mask;
	struct mid_bag* units_bag;
	struct network_interface* ifs;
	long ifs_len;
	long content_length;
	long sleep_time;
	int quit;
	int detailed_progress;
};

struct scheduler_info
{
	struct interface_report* current;
	struct network_interface* ifs;
	long ifs_len;
	long* max_speed;
	long* max_connections;
	long sleep_time;
	long sch_id;
	long max_parallel_downloads;
	int probing_done;
};

void* unit(void* info);

struct unit_info* handle_unit_errors(struct unit_info** units,long units_len);

struct interface_report* get_interface_report(struct unit_info** units,long units_len,struct network_interface* net_if,long if_len,struct interface_report* prev);

struct unit_info* largest_unit(struct unit_info** units,long units_len);

struct unit_info* idle_unit(struct unit_info** units,long units_len);

void scheduler(struct scheduler_info* sch_info);

long suspend_units(struct unit_info** units,long units_len);

void* compare_units_progress(void* unit_a,void* unit_b);

struct units_progress* merge_units_progress(struct units_progress* progress);

struct units_progress* get_units_progress(struct unit_info** units,long units_len);

void skip_progress_text(long count);

void* show_progress(void* s_progress_info);

char* convert_speed(long speed);

char* convert_data(long data,long total_data);

char* convert_time(long sec);

struct unit_info* unitdup(struct unit_info* src);

#endif /* MID_UNIT_H_ */
