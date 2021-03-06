/*
 * MID_unit.h
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
#define MID_DEFAULT_PARALLEL_DOWNLOADS 10
#define MID_DEFAULT_UNIT_BREAK_THRESHOLD_SIZE 102400
#define MID_MAX_UNIT_RETRIES 3
#define MID_DEFAULT_UNIT_RETRY_SLEEP_TIME 5
#define ERR_CODE_503_HEALING_TIME 10
#define MID_DEFAULT_PROGRESS_UPDATE_TIME 1
#define MID_DEFAULT_SCHEDULER_SLEEP_TIME 2
#define MID_DEFAULT_ALL_SCHEDULER_SLEEP_TIME 1
#define MID_MIN_ALL_SCHEDULER_SLEEP_TIME 1
#define MID_MAX_ALL_SCHEDULER_SLEEP_TIME 5
#define SCHEDULER_THRESHOLD_SPEED 2097152
#define MID_DEFAULT_SCHEDULER_ALOGORITHM maxout_scheduler

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
			free_mid_client(&mid_cli);\
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
	struct mid_interface* mid_if;
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
	struct mid_interface* ifs;
	long ifs_len;
	long content_length;
	long sleep_time;
	int quit;
	int detailed_progress;
};

struct scheduler_info
{
	struct mid_interface* ifs;
	long ifs_len;
	struct interface_report* ifs_report;
	long max_parallel_downloads;
	long sch_sleep_time;
	long sch_if_id;

	// Scheduler specific data.

	void* data;
};

typedef void (*scheduler)(struct scheduler_info*);

void* unit(void* info);

struct unit_info* handle_unit_errors(struct unit_info** units,long units_len);

struct interface_report* get_interface_report(struct unit_info** units,long units_len,struct mid_interface* net_if,long if_len,struct interface_report* prev);

struct unit_info* largest_unit(struct unit_info** units,long units_len);

struct unit_info* idle_unit(struct unit_info** units,long units_len);

void maxout_scheduler(struct scheduler_info* sch_info);

void all_scheduler(struct scheduler_info* sch_info);

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
