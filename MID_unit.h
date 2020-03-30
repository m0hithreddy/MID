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
#define MAX_UNIT_RETRIES 3
#define UNIT_RETRY_SLEEP_TIME 5
#define PROGRESS_UPDATE_TIME 1
#define SCHEDULER_DEFAULT_SLEEP_TIME 2

#include"MID_http.h"
#include"MID_socket.h"
#include"MID_interfaces.h"
#include<netinet/in.h>
#include<time.h>
#include<pthread.h>

struct unit_info
{
	pthread_t unit_id;
	pthread_mutex_t lock;
	char* file;
	char* if_name;
	long if_id;
	long current_size; // Amount downloaded in a given range
	long total_size;   // Total data fetched by unit
	long* report_size; // Total data fetched by unit from each interface, to compute if_report;
	long report_len;
	long max_unit_retries;
	int resume;
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
	struct sockaddr* servaddr;
	struct http_request* s_request;
	struct http_range* range;
	struct data_bag* unit_ranges; // HTTP ranges downloaded by unit
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
	struct interface_report* report;
	long report_len;
	struct units_progress* progress;
	long content_length;
	long sleep_time;
	int quit;
	int detailed_progress;
};

struct scheduler_info
{
	struct interface_report* prev;
	struct interface_report* current;
	char* ifs;
	long ifs_len;
	long sleep_time;
	long prev_sch_id;
	long max_parallel_downloads;
};

void* unit(void* info);

struct unit_info* handle_unit_errors(struct unit_info* unit);

struct interface_report* get_interface_report(struct unit_info** units,long units_len,struct network_interface* net_if,long if_len,struct interface_report* prev);

struct unit_info* largest_unit(struct unit_info** units,long units_len);

struct unit_info* idle_unit(struct unit_info** units,long units_len);

long scheduler(struct scheduler_info* sch_info,long if_id);

long suspend_units(struct unit_info** units,long units_len);

void* compare_units_progress(void* unit_a,void* unit_b);

struct units_progress* merge_units_progress(struct units_progress* progress);

struct units_progress* actual_progress(struct unit_info** units,long units_len);

void clear_progress_display(long count);

void* show_progress(void* s_progress_info);

char* convert_speed(long speed);

char* convert_data(long data,long total_data);

char* convert_time(long sec);

#endif /* MID_UNIT_H_ */
